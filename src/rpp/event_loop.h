#pragma once
/**
 * Single-threaded event loop for serializing coroutine completions.
 * An alternative to thread_pool that ensures all coroutine resumes
 * happen on the event loop thread, enabling efficient single-threaded
 * coroutine scheduling.
 *
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include "future_types.h"
#include "thread_pool.h" // parallel_task, pool_task_handle
#include "concurrent_queue.h"
#include "timer.h"
#include "threads.h"
#include "debugging.h"
#include <atomic>
#include <functional>
#include <type_traits>

#if RPP_HAS_COROUTINES

namespace rpp
{
    /**
     * @brief A single-threaded event loop that serializes coroutine completions.
     *
     * Unlike thread_pool, which resumes coroutines on background threads,
     * event_loop ensures that all coroutine resumes happen on the thread
     * that is running the loop (typically the main thread).
     *
     * Background work (such as lambdas in co_await [&]{...}) is still dispatched
     * to the global thread_pool, but when the work completes, the coroutine
     * resume is posted back to the event loop instead of running inline
     * on the worker thread.
     *
     * This enables a programming model where:
     * - Suspended coroutines are paused while other pending tasks can resume
     * - All resumes are serialized onto a single thread (no data races)
     * - The user drives the loop via run_loop() or run_once()
     *
     * @code
     *     rpp::event_loop loop;
     *
     *     rpp::cfuture<std::string> fetchData() {
     *         std::string raw = co_await loop.run_background([&]{
     *             return downloadFile(url);  // runs on thread pool
     *         });
     *         // resumed on event_loop thread
     *         co_return parseData(raw);
     *     }
     *
     *     auto future = fetchData();
     *     loop.run_loop();  // drives event processing until no work remains
     *     auto result = future.get();
     * @endcode
     */
    class RPPAPI event_loop
    {
    public:
        event_loop() noexcept;
        ~event_loop() noexcept;
        NOCOPY_NOMOVE(event_loop)

        /**
         * @brief Runs the event loop until there are no more pending tasks.
         *
         * Processes all queued coroutine resumes and waits for new ones.
         * Returns when both the resume queue is empty and no background
         * tasks are pending (pending_count == 0).
         */
        void run_loop();

        /**
         * @brief Processes at most one pending resume event.
         *
         * @param timeout Maximum time to wait for a resume event.
         *                Use Duration::zero() for non-blocking poll.
         * @returns true if a resume was processed, false if timed out
         */
        bool run_once(rpp::Duration timeout);

        /**
         * @brief Returns true if the event loop has pending work.
         * This includes both queued resumes and background tasks
         * that haven't posted their resume yet.
         */
        bool has_pending_work() const noexcept
        {
            return pending_count.load(std::memory_order_acquire) > 0;
        }

        /**
         * @brief Returns the number of pending tasks (background + queued resumes).
         */
        int pending_tasks() const noexcept
        {
            return pending_count.load(std::memory_order_acquire);
        }

        /**
         * @brief Returns the thread ID of the event loop thread.
         * This is set when run_loop() or run_once() is first called.
         */
        uint64 loop_thread_id() const noexcept
        {
            return owner_thread_id.load(std::memory_order_acquire);
        }

        /**
         * @brief Posts a coroutine handle to be resumed on the event loop thread.
         * Thread-safe: can be called from any thread.
         */
        void post_resume(rpp::coro_handle<> handle) noexcept;

        /**
         * @brief Posts a generic callback to be executed on the event loop thread.
         * Thread-safe: can be called from any thread.
         */
        void post(rpp::delegate<void()> callback) noexcept;

        // ─── Awaiter types ──────────────────────────────────────────

        /**
         * @brief Awaiter that runs a void lambda on the thread pool and resumes
         *        the coroutine on the event loop thread.
         */
        struct background_awaiter_void
        {
            event_loop& loop;
            rpp::delegate<void()> action;
            std::exception_ptr ex {};

            background_awaiter_void(event_loop& loop, rpp::delegate<void()> action) noexcept
                : loop{loop}, action{std::move(action)} {}

            bool await_ready() const noexcept { return false; }

            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                loop.pending_count.fetch_add(1, std::memory_order_acq_rel);
                rpp::parallel_task_detached([this, cont]() mutable
                {
                    try { action(); }
                    catch (...) { ex = std::current_exception(); }
                    loop.post_resume(cont);
                });
            }

            void await_resume()
            {
                if (ex) std::rethrow_exception(ex);
            }
        };

        /**
         * @brief Awaiter that runs a lambda on the thread pool and resumes
         *        the coroutine on the event loop thread.
         *
         * @code
         *     std::string result = co_await loop.run_background([&]{
         *         return expensiveComputation();
         *     });
         * @endcode
         */
        template<typename T>
        struct background_awaiter
        {
            event_loop& loop;
            rpp::delegate<T()> action;
            T result {};
            std::exception_ptr ex {};

            background_awaiter(event_loop& loop, rpp::delegate<T()> action) noexcept
                : loop{loop}, action{std::move(action)} {}

            bool await_ready() const noexcept { return false; }

            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                loop.pending_count.fetch_add(1, std::memory_order_acq_rel);
                rpp::parallel_task_detached([this, cont]() mutable
                {
                    try { result = action(); }
                    catch (...) { ex = std::current_exception(); }
                    loop.post_resume(cont);
                });
            }

            T await_resume()
            {
                if (ex) std::rethrow_exception(ex);
                return std::move(result);
            }
        };

        /**
         * @brief Creates an awaiter that runs the given lambda on the thread pool
         *        and resumes the coroutine on the event loop thread.
         */
        template<typename Func>
        auto run_background(Func&& func) noexcept
        {
            using R = decltype(func());
            if constexpr (std::is_void_v<R>)
                return background_awaiter_void{ *this, std::forward<Func>(func) };
            else
                return background_awaiter<R>{ *this, std::forward<Func>(func) };
        }

        // ─── Future awaiter ─────────────────────────────────────────

        /**
         * @brief Awaiter that waits for a cfuture on a background thread,
         *        then resumes the coroutine on the event loop thread.
         */
        template<typename T>
        struct future_awaiter
        {
            event_loop& loop;
            rpp::cfuture<T>& fut;
            std::exception_ptr ex {};

            future_awaiter(event_loop& loop, rpp::cfuture<T>& fut) noexcept
                : loop{loop}, fut{fut} {}

            bool await_ready() const noexcept
            {
                return fut.valid()
                    && fut.wait_for(std::chrono::microseconds{0}) != std::future_status::timeout;
            }

            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                if (!fut.valid())
                {
                    // nothing to wait on; resume immediately on loop thread
                    loop.post_resume(cont);
                    return;
                }
                loop.pending_count.fetch_add(1, std::memory_order_acq_rel);
                rpp::parallel_task_detached([this, cont]() mutable
                {
                    try { fut.wait(); }
                    catch (...) { ex = std::current_exception(); }
                    loop.post_resume(cont);
                });
            }

            auto await_resume()
            {
                if (ex) std::rethrow_exception(ex);
                return fut.get();
            }
        };

        /**
         * @brief Creates an awaiter that waits for a future on a background thread,
         *        then resumes on the event loop thread.
         * @code
         *     auto result = co_await loop.await_future(myFuture);
         * @endcode
         */
        template<typename T>
        future_awaiter<T> await_future(rpp::cfuture<T>& fut) noexcept
        {
            return future_awaiter<T>{ *this, fut };
        }

        // ─── Sleep / delay awaiter ──────────────────────────────────

        /**
         * @brief Awaiter that sleeps on a background thread, then resumes
         *        on the event loop thread.
         */
        template<class Clock = std::chrono::high_resolution_clock>
        struct delay_awaiter
        {
            event_loop& loop;
            typename Clock::time_point end;

            template<typename Rep, typename Period>
            delay_awaiter(event_loop& loop, std::chrono::duration<Rep, Period> d) noexcept
                : loop{loop}, end{Clock::now() + std::chrono::duration_cast<typename Clock::duration>(d)} {}

            bool await_ready() const noexcept { return Clock::now() >= end; }

            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                loop.pending_count.fetch_add(1, std::memory_order_acq_rel);
                rpp::parallel_task_detached([this, cont]() mutable
                {
                    std::this_thread::sleep_until(end);
                    loop.post_resume(cont);
                });
            }

            void await_resume() const noexcept {}
        };

        /**
         * @brief Creates an awaiter that sleeps for the given duration,
         *        then resumes on the event loop thread.
         * @code
         *     co_await loop.delay(std::chrono::milliseconds{100});
         * @endcode
         */
        template<typename Rep, typename Period>
        delay_awaiter<> delay(std::chrono::duration<Rep, Period> duration) noexcept
        {
            return delay_awaiter<>{ *this, duration };
        }

    private:
        friend class event_loop_test;

        struct resume_event
        {
            rpp::coro_handle<> handle {};
            rpp::delegate<void()> callback {};

            // resume via coroutine handle
            explicit resume_event(rpp::coro_handle<> h) noexcept : handle{h} {}
            // resume via generic callback
            explicit resume_event(rpp::delegate<void()> cb) noexcept : callback{std::move(cb)} {}
            resume_event() noexcept = default;
        };

        // the thread that owns and drives this event loop
        std::atomic<uint64> owner_thread_id {0};

        // number of pending tasks: background tasks that haven't completed + queued resumes
        std::atomic_int pending_count {0};

        // thread-safe FIFO queue of resume events
        rpp::concurrent_queue<resume_event> resume_queue;

        // processes a single resume event
        void process_event(resume_event& event) noexcept;
    };

} // namespace rpp

#endif // RPP_HAS_COROUTINES

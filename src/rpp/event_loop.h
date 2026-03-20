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
            ~resume_event() noexcept = default;
        };

        // the thread that owns and drives this event loop, initialized in CTOR
        std::atomic_uint64_t owner_thread_id {0};

        // number of tasks currently suspended in a background task
        std::atomic_int num_background_suspended {0};

        // true if the infinite run_loop() was started, instead of run_once()
        std::atomic_bool loop_running { false };

        rpp::thread_pool& background_pool;

        // thread-safe FIFO queue of resume events
        rpp::concurrent_queue<resume_event> resume_queue;

        // user-provided exception handler for any unhandled exceptions from background tasks
        rpp::delegate<void(std::exception_ptr)> except_handler {};

    public:
        /**
         * @brief Initializes a new event loop.
         * @param main_thr_id Captures the main thread ID where the loop will supposedly run.
         * @param background_task_pool Optional thread pool for running background tasks.
         *                             If null, then global thread pool is used.
         */
        event_loop(rpp::uint64 main_thr_id = 0/*0=rpp::get_thread_id()*/,
                   rpp::thread_pool* background_task_pool = nullptr) noexcept;
        ~event_loop() noexcept;
        NOCOPY_NOMOVE(event_loop)

        /** @returns true if there are background tasks currently in progress */
        bool has_background_tasks() const noexcept { return num_background_suspended.load(std::memory_order_acquire) > 0; }

        /** @returns the number of pending background tasks. */
        int background_tasks() const noexcept { return num_background_suspended.load(std::memory_order_acquire); }

        /** @returns true if there are any pending resume events for the main thread to handle */
        bool has_pending_completions() const noexcept { return !resume_queue.empty(); }

        /** @returns the number of pending resume events for the main thread. */
        int pending_completions() const noexcept { return int(resume_queue.size()); }

        /** @returns true if there are any pending tasks or resume events that the loop should process */
        bool has_pending_work() const noexcept { return has_background_tasks() || has_pending_completions(); }

        /** @returns the thread ID of the event loop thread. Set in CTOR. */
        rpp::uint64 main_thread_id() const noexcept { return owner_thread_id.load(std::memory_order_acquire); }

        /**
         * @brief Signals the event loop to return and finalize all pending tasks.
         */
        void stop() noexcept;

        /**
         * @brief Waits on all pending tasks to fully drain the event loop
         * @param timeout Maximum time to wait for pending tasks to complete.
         * @returns true if all tasks were completed within timeout
         */
        bool wait_on_all(rpp::Duration timeout = rpp::seconds(1)) noexcept;

        /**
         * @brief By default exceptions are swallowed and logged as warnings.
         * This allows the event loop to handle these errors without crashing.
         */
        void set_except_handler(rpp::delegate<void(std::exception_ptr)> handler) noexcept
        { except_handler = std::move(handler); }

        /**
         * @brief Runs the event loop until stop() is called and all pending tasks are completed.
         *
         * Processes all queued coroutine resumes and waits for new ones.
         * Returns when stop() is called and attempts to drain any remaining pending tasks before exiting.
         * @returns true if all pending tasks were completed, false if some tasks still pending
         */
        bool run_loop() noexcept;

        /**
         * @brief Processes at most one pending resume event.
         *
         * @param timeout Maximum time to wait for a resume event.
         *                Use Duration::zero() for non-blocking poll.
         * @returns true if a resume was processed, false if timed out
         */
        bool run_once(rpp::Duration timeout) noexcept;

        /**
         * @brief Runs the event loop until there are no more background tasks and nore more resume events
         * @returns Number of resume events processed before the loop became idle
         */
        int run_until_idle() noexcept;

        /**
         * @brief Posts a coroutine handle to be resumed on the event loop thread.
         * Thread-safe: can be called from any thread.
         */
        void post_resume(rpp::coro_handle<> handle) noexcept;

        /**
         * @brief Posts a generic callback to be executed on the event loop thread.
         * Thread-safe: can be called from any thread.
         * 
         * This is often known as `run_on_main_thread()` in GUI frameworks.
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
                loop.num_background_suspended.fetch_add(1, std::memory_order_acq_rel);
                rpp::parallel_task_detached([this, cont]() mutable
                {
                    try { action(); }
                    catch (...) { ex = std::current_exception(); }
                    loop.num_background_suspended.fetch_sub(1, std::memory_order_acq_rel);
                    loop.post_resume(cont);
                });
            }
            void await_resume() { if (ex) std::rethrow_exception(ex); }
        };

        /**
         * @brief Awaiter that runs a lambda on the thread pool and resumes
         *        the coroutine on the event loop thread.
         */
        template<typename T> struct background_awaiter
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
                loop.num_background_suspended.fetch_add(1, std::memory_order_acq_rel);
                rpp::parallel_task_detached([this, cont]() mutable
                {
                    try { result = action(); }
                    catch (...) { ex = std::current_exception(); }
                    loop.num_background_suspended.fetch_sub(1, std::memory_order_acq_rel);
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
         * @code
         *     std::string result = co_await loop.run_background([&]{
         *         return expensiveComputation();
         *     });
         * @endcode
         */
        template<typename Func>
        auto run_background(Func&& func) noexcept
        {
            using R = decltype(func());
            if constexpr (std::is_void_v<R>) return background_awaiter_void{ *this, std::forward<Func>(func) };
            else                             return background_awaiter<R>{ *this, std::forward<Func>(func) };
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
                return fut.valid() && fut.wait_for(std::chrono::microseconds{0}) != std::future_status::timeout;
            }
            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                if (!fut.valid())
                {
                    // nothing to wait on; resume immediately on loop thread
                    loop.post_resume(cont);
                    return;
                }
                loop.num_background_suspended.fetch_add(1, std::memory_order_acq_rel);
                rpp::parallel_task_detached([this, cont]() mutable
                {
                    try {
                        if (fut.valid())
                            fut.wait(); 
                    } catch (...) { ex = std::current_exception(); }
                    loop.num_background_suspended.fetch_sub(1, std::memory_order_acq_rel);
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
        struct delay_awaiter
        {
            event_loop& loop;
            rpp::TimePoint end;
            delay_awaiter(event_loop& loop, rpp::TimePoint tp) noexcept : loop{loop}, end{tp} {}
            delay_awaiter(event_loop& loop, rpp::Duration d) noexcept : loop{loop}, end{rpp::TimePoint::now() + d} {}
            bool await_ready() const noexcept { return rpp::TimePoint::now() >= end; }
            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                loop.num_background_suspended.fetch_add(1, std::memory_order_acq_rel);
                rpp::parallel_task_detached([this, cont]() mutable
                {
                    rpp::sleep_until(end);
                    loop.num_background_suspended.fetch_sub(1, std::memory_order_acq_rel);
                    loop.post_resume(cont);
                });
            }
            void await_resume() const noexcept {}
        };

        /**
         * @brief Creates an awaiter that sleeps for the given duration,
         *        then resumes on the event loop thread.
         * @code
         *     co_await loop.delay(rpp::millis(100));
         * @endcode
         */
        delay_awaiter delay(rpp::Duration duration) noexcept
        {
            return delay_awaiter{ *this, duration };
        }
        delay_awaiter delay_until(rpp::TimePoint until) noexcept
        {
            return delay_awaiter{ *this, until };
        }

    private:

        // processes a single resume event
        void process_event(resume_event& event) noexcept;
    };

} // namespace rpp

#endif // RPP_HAS_COROUTINES

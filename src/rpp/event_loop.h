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
#include "collections.h" // rpp::erase_if
#include "timer.h"
#include "threads.h"
#include "debugging.h"
#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>

#if RPP_HAS_COROUTINES

namespace rpp
{
    /**
     * @brief A simple top-level coroutine return type for use with event_loop.
     *
     * `event_task` acts as a fire-and-forget coroutine handle that the event loop
     * drives. It starts eagerly (no initial suspension), suspends at final_suspend
     * so the caller can query completion, and captures unhandled exceptions.
     *
     * Usage:
     * @code
     *     rpp::event_task myCoroutine(rpp::event_loop& loop) {
     *         int val = co_await loop.run_async([]{ return heavyWork(); });
     *         // resumed on event loop thread
     *         updateUI(val);
     *     }
     *
     *     rpp::event_loop loop;
     *     auto task = myCoroutine(loop);
     *     loop.run_until_done(task); // drives the loop and rethrows on failure
     * @endcode
     */
    struct RPPAPI RPP_CORO_RETURN_TYPE event_task
    {
        struct promise_type
        {
            std::exception_ptr exception;
            rpp::delegate<void()> on_complete {}; // optional: called at final_suspend (e.g. by fork())
            event_task get_return_object() noexcept
            {
                return event_task{rpp::coro_handle<promise_type>::from_promise(*this)};
            }
            rpp::suspend_never initial_suspend() noexcept { return {}; }
            // custom final_awaiter: calls on_complete before suspending (keeps frame alive)
            struct final_awaiter
            {
                bool await_ready() noexcept { return false; }
                void await_suspend(rpp::coro_handle<promise_type> h) noexcept
                {
                    if (auto& promise_completed = h.promise().on_complete)
                        promise_completed();
                }
                void await_resume() noexcept {}
            };
            final_awaiter final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() noexcept { exception = std::current_exception(); }
        };

        rpp::coro_handle<promise_type> handle;

        explicit event_task(rpp::coro_handle<promise_type> h) noexcept : handle{h} {}
        event_task(event_task&& o) noexcept : handle{o.handle} { o.handle = nullptr; }
        ~event_task() { if (handle) handle.destroy(); }
        event_task& operator=(event_task&& o) noexcept
        {
            if (this != &o) { if (handle) handle.destroy(); handle = o.handle; o.handle = nullptr; }
            return *this;
        }
        event_task(const event_task&) = delete;
        event_task& operator=(const event_task&) = delete;

        /** @returns true if the coroutine has finished (or was never started) */
        bool done() const noexcept { return !handle || handle.done(); }

        /** @brief Rethrows any unhandled exception captured by the coroutine */
        void rethrow_if_exception() const
        {
            if (handle && handle.promise().exception)
            {
                std::exception_ptr ex = std::move(handle.promise().exception); // only throw once
                std::rethrow_exception(ex);
            }
        }
    };

    /**
     * @brief A single-threaded event loop that serializes coroutine completions.
     *
     * Unlike thread_pool, which resumes coroutines on background threads,
     * event_loop ensures that all coroutine resumes happen on the thread
     * that is running the loop (typically the main thread).
     *
     * Background work (such as lambdas in co_await [&]{...}) is still dispatched
     * to the configured thread_pool, but when the work completes, the coroutine
     * resume is posted back to the event loop instead of running inline
     * on the worker thread.
     *
     * This enables a programming model where:
     * - Suspended coroutines are paused while other pending tasks can resume
     * - All resumes are serialized onto a single thread (no data races)
     * - The user drives the loop via run_once(), run_until_idle() or run_loop()
     * - fork() launches concurrent coroutine paths with event-driven join_forks()
     * - run_async() supports semaphore waits and queue pops (resumes on loop thread)
     *
     * @code
     *     rpp::event_loop loop;
     *
     *     rpp::cfuture<std::string> fetchData() {
     *         std::string raw = co_await loop.run_async([&]{
     *             return downloadFile(url);  // runs on thread pool
     *         });
     *         // NOTE: always resumed on event_loop thread, parse data on main thread
     *         //       the coroutine can be cancelled by throwing an exception in downloadFile()
     *         co_return parseData(raw);
     *     }
     *
     *     auto future = fetchData();
     *     loop.run_until_idle();  // drives event processing until no work remains
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

        // fork tracking: stores event_tasks for forked coroutines
        std::vector<event_task> fork_tasks;
        std::atomic<int> num_active_forks {0};
        rpp::coro_handle<> fork_joiner {}; // coroutine waiting in join_forks()

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
         * Automatically cleans up completed forks; fork exceptions go through except_handler.
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
         * @brief Runs the event loop until there are no more background tasks and no more resume events.
         *
         * Automatically cleans up completed forks; fork exceptions go through except_handler.
         * @returns Number of resume events processed before the loop became idle
         */
        int run_until_idle() noexcept;

        /**
         * @brief Drives the event loop until the given event_task completes,
         *        then rethrows any exception captured by the coroutine.
         *
         * This is the simplest way to run a top-level event_task coroutine:
         * @code
         *     auto task = myCoroutine(loop);
         *     loop.run_until_done(task);
         * @endcode
         */
        void run_until_done(event_task& task);

    private:
        // forward declarations for fork API (used by fork() template)
        void cleanup_forks() noexcept;
        void notify_fork_joiner() noexcept;

    public:
        // ─── Fork API ────────────────────────────────────────────

        /**
         * @brief Forks a new coroutine execution path on the event loop.
         *
         * The callback is invoked immediately on the event loop thread (eager start).
         * It must return `rpp::event_task`. The resulting coroutine is tracked
         * internally — no handle management needed.
         *
         * Must be called on the event loop thread.
         *
         * @code
         *     loop.fork([&]() -> rpp::event_task {
         *         auto result = co_await loop.run_async([&]{ return heavyWork(); });
         *         updateState(result);
         *     });
         *     loop.fork([&]() -> rpp::event_task { ... });
         *     loop.run_until_idle();  // drives all forks, cleans up on completion
         * @endcode
         */
        template<typename F>
        void fork(F&& coro_factory)
        {
            // must be called on the event loop thread (fork_tasks is not thread-safe)
            if (rpp::get_thread_id() != owner_thread_id.load(std::memory_order_acquire))
            {
                LogError("event_loop::fork() must be called on the event loop thread");
                return;
            }

            // clean up completed forks to avoid unbounded growth
            cleanup_forks();

            num_active_forks.fetch_add(1, std::memory_order_acq_rel);

            // Heap-allocate the lambda to keep it alive for the coroutine's lifetime.
            // Lambda coroutines store a `this` pointer to the lambda object — if the
            // lambda is a temporary and is destroyed before the coroutine completes,
            // all by-reference captures dangle.
            auto stored = std::make_shared<std::decay_t<F>>(std::forward<F>(coro_factory));
            auto task = (*stored)(); // starts eagerly on event loop thread

            if (task.done())
            {
                // completed synchronously (no co_await in callback)
                num_active_forks.fetch_sub(1, std::memory_order_acq_rel);
                notify_fork_joiner();
            }
            else
            {
                // Capture shared_ptr in on_complete to extend lambda lifetime.
                // The delegate lives in the promise (coroutine frame), which stays
                // alive until the event_task destructor calls handle.destroy().
                task.handle.promise().on_complete = [this, stored]()
                {
                    num_active_forks.fetch_sub(1, std::memory_order_acq_rel);
                    notify_fork_joiner();
                };
            }
            fork_tasks.push_back(std::move(task));
        }

        /**
         * @brief Returns the number of forked coroutines that have not yet completed.
         */
        int num_forks() const noexcept { return num_active_forks.load(std::memory_order_acquire); }

        /**
         * @brief Checks completed forks for exceptions and clears them.
         *
         * Rethrows the first exception found among completed forks.
         * Must be called on the event loop thread.
         */
        void drain_forks();

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
        void post(rpp::delegate<void()>&& callback) noexcept;

        // ─── Awaiter types ──────────────────────────────────────────

        /**
         * @brief Awaiter that runs a void lambda on the thread pool and resumes
         *        the coroutine on the event loop thread.
         */
        struct RPP_CORO_RETURN_TYPE background_awaiter_void
        {
            event_loop& loop;
            rpp::delegate<void()> action;
            std::exception_ptr ex {};

            background_awaiter_void(event_loop& loop, rpp::delegate<void()> action) noexcept
                : loop{loop}, action{std::move(action)} {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                loop.start_in_background([this, cont]() mutable
                {
                    try { action(); }
                    catch (...) { ex = std::current_exception(); }
                    // WARNING: do not deallocate action here, it can lead to a race-condition + memory corruption
                    loop.post_resume_from_suspension(cont);
                });
            }
            void await_resume() { if (ex) std::rethrow_exception(ex); }
        };

        /**
         * @brief Awaiter that runs a lambda on the thread pool and resumes
         *        the coroutine on the event loop thread.
         */
        template<typename T> struct RPP_CORO_RETURN_TYPE background_awaiter
        {
            event_loop& loop;
            rpp::delegate<T()> action;
            std::optional<T> result {};
            std::exception_ptr ex {};

            background_awaiter(event_loop& loop, rpp::delegate<T()> action) noexcept
                : loop{loop}, action{std::move(action)} {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                loop.start_in_background([this, cont]() mutable
                {
                    try { result.emplace(action()); }
                    catch (...) { ex = std::current_exception(); }
                    // WARNING: do not deallocate action here, it can lead to a race-condition + memory corruption
                    loop.post_resume_from_suspension(cont);
                });
            }
            T await_resume()
            {
                // release resources before resuming the coroutine
                action = {};
                if (ex) std::rethrow_exception(ex);
                return std::move(*result);
            }
        };

        /**
         * @brief Awaiter that runs a future on the thread pool and resumes 
         *        the coroutine on the event loop thread when the future is ready.
         */
        template<IsFuture Future> struct RPP_CORO_RETURN_TYPE background_awaiter_fut
        {
            event_loop& loop;
            rpp::delegate<Future()> action;
            Future f {};
            std::exception_ptr ex {};

            background_awaiter_fut(event_loop& loop, rpp::delegate<Future()> action) noexcept
                : loop{loop}, action{std::move(action)} {}
            bool await_ready() const noexcept { return false; }
            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                loop.start_in_background([this, cont]() mutable
                {
                    try { 
                        f = action(); // get the future from the lambda
                        f.wait(); // wait for the nested coroutine to finish (can throw)
                    } catch (...) { ex = std::current_exception(); }
                    // WARNING: do not deallocate action here, it can lead to a race-condition + memory corruption
                    loop.post_resume_from_suspension(cont);
                });
            }
            // similar to future<T>, either gets the result T, or throws the caught exception
            auto await_resume()
            {
                // release resources before resuming the coroutine
                action = {};
                if (ex) std::rethrow_exception(ex);
                return f.get();
            }
        };

        /**
         * @brief Awaiter that waits for a cfuture on a background thread,
         *        then resumes the coroutine on the event loop thread.
         */
        template<typename T>
        struct RPP_CORO_RETURN_TYPE future_awaiter
        {
            event_loop& loop;
            rpp::cfuture<T> fut;
            std::exception_ptr ex {};

            future_awaiter(event_loop& loop, rpp::cfuture<T>&& fut) noexcept
                : loop{loop}, fut{std::move(fut)} {}
            future_awaiter(event_loop& loop, std::future<T>&& fut) noexcept
                : loop{loop}, fut{std::move(fut)} {}

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
                loop.start_in_background([this, cont]() mutable
                {
                    try {
                        if (fut.valid())
                            fut.wait();
                    } catch (...) { ex = std::current_exception(); }
                    loop.post_resume_from_suspension(cont);
                });
            }
            auto await_resume()
            {
                if (ex) std::rethrow_exception(ex);
                if (!fut.valid())
                {
                    if constexpr (std::is_void_v<T>) return;
                    else return T{};
                }
                return fut.get();
            }
        };

        /**
         * @brief Creates an awaiter that runs the given lambda on the thread pool
         *        and resumes the coroutine on the event loop thread.
         * 
         *  NOTE: The lambda runs in a background thread context,
         *        but ALWAYS resumes on the Main Thread !
         *
         * @code
         *     std::string result = co_await loop.run_async([&]{
         *         return expensiveComputation();
         *     });
         *     // After co_await, we are back on the event loop thread
         * 
         *     std::string result2 = co_await loop.run_async([&]() -> rpp::cfuture<std::string> {
         *         auto input = prepareExpensiveInput();
         *         std::string output = co_await loop.run_async([&]{
         *            return expensiveComputation(input);
         *         });
         *         // Resumes on event loop thread !
         *         ui.showResult(output);
         *         co_return output;
         *     });
         *     // After co_await, we are back on the event loop thread
         * @endcode
         */
        template<typename FutureOrCallback>
        RPP_CORO_WRAPPER auto run_async(FutureOrCallback&& fut_or_cb) noexcept
        {
            using Decayed = std::decay_t<FutureOrCallback>;
            if constexpr (IsFuture<Decayed>) // rpp::cfuture<R> or std::future<R>
            {
                using T = decltype(fut_or_cb.get());
                return future_awaiter<T>{ *this, std::move(fut_or_cb) };
            }
            else if constexpr (IsFunctionReturningFuture<Decayed>) // lambda[]() -> rpp::cfuture<R>
            {
                using Fut = decltype(fut_or_cb());
                return background_awaiter_fut<Fut>{ *this, std::move(fut_or_cb) };
            }
            else // lambda[]()->R or rpp::delegate<R()>
            {
                using R = decltype(fut_or_cb());
                if constexpr (std::is_void_v<R>) return background_awaiter_void{ *this, std::move(fut_or_cb) };
                else                             return background_awaiter<R>{ *this, std::move(fut_or_cb) };
            }
        }

        // ─── Semaphore / Queue await ────────────────────────────────

        /**
         * @brief Waits for a semaphore signal on a background thread,
         *        resumes on the event loop thread.
         * @code
         *     auto wr = co_await loop.await(sem, rpp::millis(100));
         *     if (wr == rpp::semaphore::notified) { // signaled }
         * @endcode
         */
        RPP_CORO_WRAPPER auto await(rpp::semaphore& sem, rpp::Duration timeout) noexcept
        {
            return run_async([&sem, timeout]() { return sem.wait(timeout); });
        }

        /**
         * @brief Pops from a queue on a background thread,
         *        resumes on the event loop thread.
         * @code
         *     std::string item;
         *     bool got = co_await loop.await(queue, item, rpp::millis(100));
         * @endcode
         */
        template<typename T>
        RPP_CORO_WRAPPER auto await(rpp::concurrent_queue<T>& queue, T& out, rpp::Duration timeout) noexcept
        {
            return run_async([&queue, &out, timeout]() { return queue.wait_pop(out, timeout); });
        }

        /**
         * @brief Pops from a queue on a background thread, returns std::optional<T>,
         *        resumes on the event loop thread.
         * @code
         *     auto item = co_await loop.await_pop(queue, rpp::millis(100));
         *     if (item) { use(*item); }
         * @endcode
         */
        template<typename T>
        RPP_CORO_WRAPPER auto await_pop(rpp::concurrent_queue<T>& queue, rpp::Duration timeout) noexcept
        {
            return run_async([&queue, timeout]() -> std::optional<T> {
                T item;
                if (queue.wait_pop(item, timeout)) return std::move(item);
                return std::nullopt;
            });
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
                loop.start_in_background([this, cont]() mutable
                {
                    rpp::sleep_until(end);
                    loop.post_resume_from_suspension(cont);
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

        /**
         * @brief Awaiter that suspends the caller until all forks complete or timeout expires.
         *
         * Event-driven: fork completions resume the joiner directly via post_resume().
         * For timeout: a background timer resumes the joiner if forks haven't finished.
         */
        struct RPP_CORO_RETURN_TYPE join_forks_awaiter
        {
            event_loop& loop;
            rpp::Duration timeout;

            bool await_ready() const noexcept
            {
                return loop.num_active_forks.load(std::memory_order_acquire) == 0;
            }
            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                loop.fork_joiner = cont;
                // double-check: all forks may have completed between await_ready and here
                // (single-threaded event loop — cannot actually race, but defensive)
                if (loop.num_active_forks.load(std::memory_order_acquire) == 0)
                {
                    loop.fork_joiner = {};
                    loop.post_resume(cont);
                    return;
                }
                // start timeout timer if finite
                if (timeout < rpp::seconds(86400))
                {
                    rpp::TimePoint deadline = rpp::TimePoint::now() + timeout;
                    loop.start_in_background([&loop = loop, cont, deadline]()
                    {
                        rpp::sleep_until(deadline);
                        // post callback to event loop thread for thread-safe joiner check
                        loop.resume_queue.push(resume_event{rpp::delegate<void()>{
                            [&loop, cont]()
                            {
                                if (loop.fork_joiner == cont)
                                {
                                    loop.fork_joiner = {};
                                    cont.resume();
                                }
                            }
                        }});
                        // balance the start_in_background counter increment
                        loop.num_background_suspended.fetch_sub(1, std::memory_order_acq_rel);
                    });
                }
            }
            int await_resume()
            {
                loop.drain_forks();
                return loop.num_active_forks.load(std::memory_order_acquire);
            }
        };

        /**
         * @brief Awaitable join for all active forks, with optional timeout.
         *
         * Suspends the caller until all forks complete or the timeout expires.
         * Returns the number of forks still active (0 = all done).
         * Event-driven: zero polling overhead; fork completions resume the joiner directly.
         *
         * @code
         *     // wait indefinitely for all forks
         *     co_await loop.join_forks();
         *
         *     // wait with timeout — handle soft deadlock
         *     int remaining = co_await loop.join_forks(rpp::seconds(5));
         *     if (remaining > 0) { // some forks still running }
         * @endcode
         */
        RPP_CORO_WRAPPER join_forks_awaiter join_forks(rpp::Duration timeout = rpp::seconds(86400)) noexcept
        {
            return join_forks_awaiter{ *this, timeout };
        }

    private:

        void start_in_background(task_delegate<void()>&& generic_task) noexcept
        {
            num_background_suspended.fetch_add(1, std::memory_order_acq_rel);
            background_pool.parallel_task_detached(std::move(generic_task));
        }

        // posts a resume event and decrements the background suspension count
        void post_resume_from_suspension(rpp::coro_handle<> handle) noexcept;

        // processes a single resume event
        void process_event(resume_event& event) noexcept;


    };

} // namespace rpp

#endif // RPP_HAS_COROUTINES

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
#include "atomic_timepoint.h" // rpp::AtomicTimeSource (optional warpable clock)
#include "collections.h" // rpp::erase_if
#include "concurrent_queue.h"
#include "config.h"
#include "debugging.h"
#include "future_types.h" // rpp::cfuture, rpp::coro_handle, rpp::suspend_never
#include "future.h" // rpp::IsFuture, rpp::IsFunctionReturningFuture
#include "task.h" // rpp::task<T> (driven to completion by run_until_done)
#include "thread_pool.h" // parallel_task, pool_task_handle
#include "timepoint.h" // rpp::Duration
#include "delegate.h" // rpp::delegate
#include "semaphore.h" // rpp::semaphore
#include "sockets.h" // rpp::socket, rpp::ipaddress
#include "threads.h"
#include "source_loc.h" // rpp::source_loc
#include <atomic>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>


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
            resume_event() noexcept = default; // no user destructor, so the timer queue can move an event out
        };

    public:
        /** @brief A snapshot of the loop clock, so a detached time source cannot strand a waiter. */
        struct time_frame
        {
            rpp::int64 offset_ns = 0; // combined sync and warp offset at capture time
            rpp::uint32 generation = 0; // the clock which built this frame, so a swap cannot move it
            bool warpable = false; // a time source was attached at capture time
            rpp::ClockType base_clock = rpp::ClockType::Monotonic; // the source clock, Monotonic without a source

            time_frame() noexcept = default;
            explicit time_frame(const rpp::AtomicTimeSource* src) noexcept
                : offset_ns{src ? src->total_offset().nsec : 0}, warpable{src != nullptr}
                , base_clock{src ? src->get_base_clock() : rpp::ClockType::Monotonic} {}

            /** @returns the time on this frame's clock: the one definition this loop uses. */
            rpp::TimePoint now() const noexcept
            {
                return rpp::TimePoint{ rpp::TimePoint::now(base_clock).duration.nsec + offset_ns };
            }
        };

        struct socket_awaiter; // a socket wait, which the loop completes through its `ready` field

    private:
        // a delay() or join_forks() deadline the loop thread owns, see wait_next_event()
        struct timer
        {
            rpp::TimePoint end; // deadline on `frame`
            time_frame frame; // the clock snapshot which built `end`
            rpp::coro_handle<> owner; // the coroutine the deadline belongs to, so it can cancel it
            resume_event event; // goes onto the resume queue when `end` passes
        };

        // a wait_readable() or wait_writable() the loop thread owns, see wait_next_event()
        struct socket_waiter
        {
            socket_awaiter* awaiter; // lives in the suspended coroutine frame until the loop resumes it
            rpp::coro_handle<> cont;
        };

        // the thread that owns and drives this event loop, initialized in CTOR
        std::atomic_uint64_t owner_thread_id {0};

        // number of tasks currently suspended in a background task
        std::atomic_int num_background_suspended {0};

        // true if the infinite run_loop() was started, instead of run_once()
        std::atomic_bool loop_running { false };

        rpp::thread_pool& background_pool;

        // optional warpable clock: when set, delay()/delay_until() track this source's
        // virtual time so warp_forward() can advance a pending wait. Null = wall clock.
        std::atomic<rpp::AtomicTimeSource*> time_source { nullptr };

        // number of threads reading through time_source right now
        mutable std::atomic_int time_source_readers { 0 };

        // bumps on every set_time_source(), so a frame can tell which clock built its offset
        std::atomic<rpp::uint32> time_source_generation { 0 };

        // thread-safe FIFO queue of resume events
        rpp::concurrent_queue<resume_event> resume_queue;

        // user-provided exception handler for any unhandled exceptions from background tasks
        rpp::delegate<void(std::exception_ptr)> except_handler {};
        rpp::delegate<void(std::exception)> std_except_handler {};

        // loop hook handler which is called every time we process run_once() 
        // or do a single iteration of run_until_idle()
        // Designed for additional test hooks, e.g. time warping fast forwarding, etc.
        rpp::delegate<void()> loop_hook_handler {};

        // fork tracking: stores event_tasks for forked coroutines
        std::vector<event_task> fork_tasks;
        std::atomic<int> num_active_forks {0};
        rpp::coro_handle<> fork_joiner {}; // coroutine waiting in join_forks()

        // the timers and socket waits the loop thread owns, see wait_next_event()
        std::vector<timer> timers;
        std::vector<socket_waiter> socket_waiters;
        std::atomic_int num_waiters {0}; // both lists, so has_pending_work() reads it from any thread
        rpp::socket_poller poller; // polls the socket waiters, and a post() wakes it from any thread

    public:
        /**
         * @brief Initializes a new event loop.
         * @param main_thr_id Captures the main thread ID where the loop will supposedly run.
         * @param background_task_pool Optional thread pool for running background tasks.
         *                             If null, then global thread pool is used.
         *                             The loop borrows it and MUST NOT outlive it. A pool
         *                             with a shorter scope leaves a background worker
         *                             touching freed state.
         * @param warpable_clock Optional clock for delay() and delay_until().
         *                       The loop borrows it and MUST NOT outlive it.
         */
        event_loop(rpp::uint64 main_thr_id = 0/*0=rpp::get_thread_id()*/,
                   rpp::thread_pool* background_task_pool RPP_LIFETIMEBOUND = nullptr,
                   rpp::AtomicTimeSource* warpable_clock RPP_LIFETIMEBOUND = nullptr) noexcept;
        /** @brief The destructor drains pending work and reads the time source, so the owner must
         *         outlive the loop. stop_and_wait_all_ready() detaches the time source first. */
        ~event_loop() noexcept;
        NOCOPY_NOMOVE(event_loop)

        // ─── the loop clock ─────────────────────────────────────────

        /**
         * @brief Attaches a warpable clock used by delay()/delay_until(). When set, a pending
         *        delay tracks this source's virtual time, so warp_forward() advances the wait.
         *        Pass null to revert to wall-clock timing. Every call waits for the readers
         *        of the old clock to drop it, so the caller may then destroy it.
         *        A pending deadline keeps the clock which built it, so a swap never moves it.
         *        The loop borrows the clock and MUST NOT outlive it. No attribute states
         *        that: clang rejects lifetimebound on a function that returns void.
         */
        void set_time_source(rpp::AtomicTimeSource* clock) noexcept
        {
            // the drain runs between the clear and the bump, so no reader pairs the old
            // clock with a newer generation. That pair moves a deadline off its timeline
            time_source.store(nullptr, std::memory_order_seq_cst);
            drain_time_source_readers();
            time_source_generation.fetch_add(1, std::memory_order_seq_cst);
            time_source.store(clock, std::memory_order_seq_cst);
        }

        /** @returns the loop's current time: the warpable clock's virtual time if attached,
         *           otherwise the monotonic wall clock. */
        rpp::TimePoint current_time() const noexcept { return get_time_source_frame().now(); }

        /** @returns the virtual time of `src`, or the monotonic wall clock when it is null. */
        static rpp::TimePoint current_time(const rpp::AtomicTimeSource* src) noexcept { return time_frame{src}.now(); }

        /** @brief Refreshes `frame` from the live clock, which any swap of that clock leaves alone.
         *  @returns the current time on that frame's clock. */
        rpp::TimePoint current_time(time_frame& frame) const noexcept
        {
            time_frame live = frame;
            read_time_source(live);
            if (live.generation == frame.generation) // a swap leaves the frame on the clock which built it
                frame = live;
            return frame.now();
        }

        /** @returns a snapshot of the loop clock, taken on the thread which builds a deadline. */
        time_frame get_time_source_frame() const noexcept;

    private:
        // reads the live source and its generation into `frame` while the reader guard is up
        void read_time_source(time_frame& frame) const noexcept;

        // waits for every reader to drop the old clock, so the owner may free it
        void drain_time_source_readers() const noexcept;

        // pops until `deadline` on the clock `frame` captured, so a drained callback
        // which frees the clock leaves no raw source inside the wait
        bool wait_pop_until(resume_event& event, rpp::TimePoint deadline, time_frame& frame) noexcept;

        // pops the next resume event until `deadline` on `frame`, but wakes at the earliest timer
        // or socket deadline first, so every wait of the loop fires them without a worker
        bool wait_next_event(resume_event& event, rpp::TimePoint deadline, time_frame& frame) noexcept;
        bool wait_next_event(resume_event& event, rpp::Duration timeout) noexcept;

        bool on_owner_thread() const noexcept { return rpp::get_thread_id() == owner_thread_id.load(std::memory_order_acquire); }

        // runs `f` now on the owner thread, and posts it there from any other thread
        template<class F> void run_on_loop(F&& f) noexcept
        {
            if (on_owner_thread()) f();
            else post(std::forward<F>(f));
        }

        // polls the socket waiters until `until`, then completes the ready ones
        bool poll_sockets_until(resume_event& event, rpp::TimePoint until, time_frame& frame) noexcept;
        // the wall time one poll may block, which is one slice when a warp or a post cannot end it
        rpp::Duration poll_wait(rpp::TimePoint until, time_frame& frame) noexcept;

        // time left until the earliest timer or socket deadline, each on the clock which built it
        rpp::Duration time_to_next_waiter() noexcept;

        // moves every timer and socket wait whose deadline passed onto the resume queue
        void fire_due_waiters() noexcept;

        // the waiter lists belong to the loop thread, so a call from another thread runs through run_on_loop()
        void add_timer(rpp::TimePoint end, time_frame frame, rpp::coro_handle<> owner, resume_event event) noexcept;
        void cancel_timers(rpp::coro_handle<> owner) noexcept;
        void add_socket_waiter(socket_awaiter& awaiter, rpp::coro_handle<> cont) noexcept;
        void complete_socket_waiter(socket_waiter& waiter, bool ready) noexcept;

    public:
        // ─── end of the loop clock ──────────────────────────────────

        /** @returns true if there are background tasks currently in progress */
        bool has_background_tasks() const noexcept { return num_background_suspended.load(std::memory_order_acquire) > 0; }

        /** @returns the number of pending background tasks. */
        int background_tasks() const noexcept { return num_background_suspended.load(std::memory_order_acquire); }

        /** @returns true if there are any pending resume events for the main thread to handle */
        bool has_pending_completions() const noexcept { return !resume_queue.empty(); }

        /** @returns the number of pending resume events for the main thread. */
        int pending_completions() const noexcept { return int(resume_queue.size()); }

        /** @returns the number of delay() timers and socket waits pending on the loop thread */
        int pending_waiters() const noexcept { return num_waiters.load(std::memory_order_acquire); }

        /** @returns true if there are any pending tasks, resume events or loop waiters that the loop should process */
        bool has_pending_work() const noexcept
        {
            return has_background_tasks() || has_pending_completions() || pending_waiters() > 0;
        }

        /** @returns the thread ID of the event loop thread. Set in CTOR. */
        rpp::uint64 main_thread_id() const noexcept { return owner_thread_id.load(std::memory_order_acquire); }

        /**
         * @brief Signals the event loop to return and finalize all pending tasks.
         */
        void stop() noexcept;

        /**
         * @brief Waits on all pending tasks to fully drain the event loop.
         *        This is not a complete drain. A resume queued as the background count reaches
         *        zero stays queued, so a shutdown path wants stop_and_wait_all_ready() instead.
         * @param timeout Maximum time to wait for pending tasks to complete.
         * @returns true if all tasks were completed within timeout
         */
        bool wait_on_all(rpp::Duration timeout = rpp::seconds(1)) noexcept;

        /**
         * @brief Stops the loop, waits for the background tasks, runs every resume they
         *        queued, and detaches the time source. The destructor then touches nothing
         *        the owner may already have destroyed.
         *        On timeout the time source stays attached, because a live delay() worker
         *        polls it against a virtual deadline and would never reach a wall-clock one.
         *        Call it on the loop thread, because the drain resumes coroutines.
         * @param max_wait Maximum time to wait for the background tasks.
         * @returns true when every task finished inside max_wait and nothing is left queued
         */
        bool stop_and_wait_all_ready(rpp::Duration max_wait = rpp::seconds(1)) noexcept;

        /**
         * @brief By default exceptions are swallowed and logged as warnings.
         * This allows the event loop to handle these errors without crashing.
         */
        void set_except_handler(rpp::delegate<void(std::exception_ptr)> handler) noexcept
        { except_handler = std::move(handler); }
        /**
         * @brief Directly collects only std::exception derived exceptions, 
         *        to simplify error handling for common cases.
         */
        void set_std_except_handler(rpp::delegate<void(std::exception)> handler) noexcept
        { std_except_handler = std::move(handler); }

        /**
         * @brief Sets a loop hook handler that is called every time we process run_once()
         *        or do a single iteration of run_until_idle().
         *        Designed for additional test hooks, e.g. time warping fast forwarding, etc.
         *        The handler is called on the event loop thread, so it can safely access
         *        any data that is only valid on the loop thread.
         *        The handler should not block or perform long-running work, as it will
         *        delay the processing of other pending tasks.
         *        
         *        The handler should not throw exceptions, as they will be caught and logged.
         *        The handler should not call run_loop() or run_once(), as this will cause
         *        a deadlock. The handler should not call stop(), as this will cause
         *        the loop to exit prematurely.
         */
        void set_loop_hook_handler(rpp::delegate<void()> handler) noexcept
        { loop_hook_handler = std::move(handler); }

        /**
         * @brief Runs the event loop until stop() is called and all pending tasks are completed.
         * @param suspend_interval Maximum time to wait suspended between loop hook invocations.
         *
         * Processes all queued coroutine resumes and waits for new ones.
         * Returns when stop() is called and attempts to drain any remaining pending tasks before exiting.
         * Automatically cleans up completed forks; fork exceptions go through except_handler.
         * @returns true if all pending tasks were completed, false if some tasks still pending
         */
        bool run_loop(rpp::Duration suspend_interval = rpp::millis(15)) noexcept;

        /**
         * @brief Processes at most one pending resume event.
         *
         * @param timeout Maximum time to wait for a resume event.
         *                Use Duration::zero() for non-blocking poll.
         *                A pending delay() or socket wait shortens the wait to its deadline.
         * @returns true if a resume was processed, false if timed out
         */
        bool run_once(rpp::Duration timeout) noexcept;

        /**
         * @brief Processes all pending resume events that are ready to run.
         *        Equivalent to while (loop.run_once(Duration::zero())) {} but more efficient.
         *        Does not block wait for new events.
         *        Does not call the loop hook handler.
         * @returns true if at least one resume was processed, false if no ready resumes were found
         */
        bool run_all_ready() noexcept;

        /**
         * @brief Runs the event loop until there are no more background tasks and no more resume events.
         * @param suspend_interval Maximum time to wait suspended between loop hook invocations.
         * Automatically cleans up completed forks; fork exceptions go through except_handler.
         * @returns Number of resume events processed before the loop became idle
         */
        int run_until_idle(rpp::Duration suspend_interval = rpp::millis(15)) noexcept;

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

        /**
         * @brief Drives the loop until the given (eager) `rpp::task<T>` completes, then returns its
         *        value (or rethrows its exception).
         *
         * The task is already running (eager); this pumps the loop so the task's background-leaf
         * continuations resume on this thread until it finishes. It does NOT block a thread — it
         * actively pumps the loop. This is the canonical way to drive a top-level task to
         * completion from non-coroutine code (e.g. tests, app startup).
         *
         * @warning Must be called on the loop's owner thread.
         * @code
         *     rpp::task<int> t = compute(loop); // eager: already in flight
         *     int result = loop.run_until_done(t);
         * @endcode
         */
        template <typename T>
        T run_until_done(rpp::task<T>& task)
        {
            return drive_to_done(task); // eager: already in flight, just pump
        }

        /**
         * @brief Drives the loop until the given (lazy) `rpp::deferred<T>` completes, then returns
         *        its value (or rethrows). Unlike `task`, a deferred has not started yet — this
         *        starts it, then pumps the loop to completion. @warning Owner-thread only.
         */
        template <typename T>
        T run_until_done(rpp::deferred<T>& task)
        {
            task.start(); // lazy: launch the body before pumping
            return drive_to_done(task);
        }

        /**
         * @brief Pumps THIS loop on its owner thread until `fut` is ready or `timeout` elapses.
         *
         * Use this to obtain an async result on the loop's OWN thread without deadlocking: the
         * awaited continuation resumes on this very thread, so a blocking `fut.get()` here would
         * wait for work only this thread can run. Pumping drives just enough of the loop to settle
         * `fut` (and whatever it depends on) — it does NOT drain the whole loop, so unrelated work
         * and other components' tasks are left running.
         *
         * @warning Must be called on the loop's owner thread.
         * @returns true if `fut` became ready within `timeout`; false on timeout (never blocks past it).
         */
        template<typename T>
        bool pump_until_ready(rpp::cfuture<T>& fut, rpp::Duration timeout = rpp::seconds(15))
        {
            time_frame frame = get_time_source_frame();
            rpp::TimePoint end = frame.now() + timeout;
            while (fut.valid() && fut.wait_for(rpp::Duration::zero()) == wait_result::timeout)
            {
                if (current_time(frame) >= end)
                    return false;
                run_once(rpp::millis(5)); // block-wait briefly for the next continuation, then run it
            }
            // if the future is deferred, .get() needs to be called to trigger the continuation
            return fut.valid() && fut.wait_for(rpp::Duration::zero()) != wait_result::timeout;
        }

        /**
         * @brief Pumps the loop until `fut` is ready (see pump_until_ready), then returns its value.
         * @throws std::runtime_error if `fut` does not become ready within `timeout` (never blocks).
         */
        template<typename T>
        T run_until_ready(rpp::cfuture<T>& fut, rpp::Duration timeout = rpp::seconds(15))
        {
            if (!pump_until_ready(fut, timeout))
                throw std::runtime_error("event_loop::run_until_ready timed out");
            return fut.get();
        }

        /**
         * @brief Verifies the caller is running on this loop's owner thread; logs an error at the
         *        caller's source location if not. A debugging aid for confirming a coroutine
         *        continuation resumed on the expected loop thread (the common async edge-case bug).
         * @returns true if on the owner thread, false otherwise.
         */
        bool ensure_on_owner_thread(rpp::source_loc loc) const noexcept;

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
            if (!on_owner_thread())
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
         * @brief Launches a fire-and-forget coroutine onto the loop from ANY thread.
         *
         * Like fork(), but callable off the owner thread: when invoked from another thread the
         * launch is marshalled onto the owner thread (via post()) before the coroutine starts,
         * so callers never have to be on the loop thread — and never have to hand-roll a
         * post()+fork() pair. The coroutine is tracked exactly like fork(): drained by
         * run_until_idle()/wait_on_all()/the destructor, with exceptions routed to except_handler.
         *
         * @param coro_factory invocable returning rpp::event_task; co_await loop.run_async(...)
         *        inside it to do background work and resume on the owner thread.
         * @code
         *     loop.run_async_void([&]() -> rpp::event_task {   // may be called from a worker thread
         *         bool ok = co_await loop.run_async([&]{ return blockingWork(); });
         *         commit(ok);   // runs on the owner (loop) thread
         *     });
         * @endcode
         */
        template<typename CoroFactory>
        void run_async_void(CoroFactory coro_factory) noexcept
        {
            run_on_loop([this, coro_factory = std::move(coro_factory)]() mutable { fork(std::move(coro_factory)); });
        }

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
                    loop.post_resume(cont);
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
                    loop.post_resume(cont);
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
                    loop.post_resume(cont);
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
                return false; // always suspend; a ready future would otherwise resume inline off-loop
            }
            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                // already resolved (ready or invalid): hop to the loop thread, no worker needed
                if (!fut.valid() || fut.wait_for(rpp::Duration::zero()) != wait_result::timeout)
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
                    loop.post_resume(cont);
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
         * @brief Awaiter which parks the coroutine on the loop timer queue, then resumes it
         *        on the event loop thread at the deadline. No pool worker is involved.
         */
        struct delay_awaiter
        {
            event_loop& loop;
            time_frame frame; // the clock snapshot which builds and polls `end`
            rpp::TimePoint end; // deadline on that snapshot's clock
            delay_awaiter(event_loop& loop, rpp::TimePoint tp) noexcept
                : loop{loop}, frame{loop.get_time_source_frame()}, end{tp} {}
            delay_awaiter(event_loop& loop, rpp::Duration d) noexcept
                : loop{loop}, frame{loop.get_time_source_frame()}, end{frame.now() + d} {}
            bool await_ready() const noexcept { return frame.now() >= end; } // same clock which built `end`
            void await_suspend(rpp::coro_handle<> cont) noexcept { loop.add_timer(end, frame, cont, resume_event{cont}); }
            void await_resume() const noexcept {}
        };

        /**
         * @brief Creates an awaiter that sleeps for the given duration on the loop timer queue,
         *        then resumes on the event loop thread. Every wait of the loop wakes at the deadline.
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

        // ─── Socket readiness awaiters ──────────────────────────────

        /**
         * @brief Awaiter which parks the coroutine until `sock` is ready, or until the timeout passes.
         *        The loop thread polls the descriptor itself, so no pool worker is involved.
         */
        struct RPP_CORO_RETURN_TYPE socket_awaiter
        {
            event_loop& loop;
            rpp::socket& sock;
            socket::PollFlag flag; // PF_Read or PF_Write
            time_frame frame; // the clock snapshot which builds and polls `end`
            rpp::TimePoint end; // deadline on that snapshot's clock
            bool ready = false; // the loop sets it before it resumes the coroutine

            socket_awaiter(event_loop& loop, rpp::socket& sock, socket::PollFlag flag, rpp::Duration timeout) noexcept
                : loop{loop}, sock{sock}, flag{flag}, frame{loop.get_time_source_frame()}, end{frame.now() + timeout} {}
            bool await_ready() const noexcept { return false; } // the loop thread decides readiness
            void await_suspend(rpp::coro_handle<> cont) noexcept { loop.add_socket_waiter(*this, cont); }
            /** @returns true when the socket is ready or has an error, false when the timeout passed */
            bool await_resume() const noexcept { return ready; }
        };

        /**
         * @brief Suspends until `sock` has data, a closed peer or an error, then resumes on the loop thread.
         * @returns true when recv() will not block, false when `timeout` passed
         * @code
         *     if (co_await loop.wait_readable(sock, rpp::millis(500)))
         *         int n = sock.recv(buf, sizeof(buf));
         * @endcode
         */
        RPP_CORO_WRAPPER socket_awaiter wait_readable(rpp::socket& sock RPP_LIFETIMEBOUND, rpp::Duration timeout) noexcept
        {
            return socket_awaiter{ *this, sock, socket::PF_Read, timeout };
        }

        /**
         * @brief Suspends until `sock` accepts a send() or completed a connect, then resumes on the loop thread.
         * @returns true when send() will not block, false when `timeout` passed
         */
        RPP_CORO_WRAPPER socket_awaiter wait_writable(rpp::socket& sock RPP_LIFETIMEBOUND, rpp::Duration timeout) noexcept
        {
            return socket_awaiter{ *this, sock, socket::PF_Write, timeout };
        }

        /**
         * @brief Awaiter which starts a non-blocking connect, waits on the loop thread until the
         *        socket is writable, then reads the connect result. No pool worker is involved.
         */
        struct RPP_CORO_RETURN_TYPE connect_awaiter : socket_awaiter
        {
            rpp::ipaddress addr;

            connect_awaiter(event_loop& loop, rpp::socket& sock, const rpp::ipaddress& addr, rpp::Duration timeout) noexcept
                : socket_awaiter{loop, sock, socket::PF_Write, timeout}, addr{addr} {}
            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                if (sock.connect_start(addr))
                    loop.add_socket_waiter(*this, cont);
                else
                    loop.post_resume(cont); // failed at once, and `ready` stays false
            }
            /** @returns true when the socket connected inside the timeout, else false with sock.last_err() set */
            bool await_resume() const noexcept { return ready && sock.connect_finish(); }
        };

        /**
         * @brief Connects `sock` to `addr` on the loop thread, see socket::connect_start().
         *        The socket stays non-blocking, which a loop-driven socket wants.
         * @code
         *     rpp::socket sock;
         *     if (co_await loop.connect(sock, rpp::ipaddress4{"192.168.168.1", 23}, rpp::seconds(2)))
         *         sock.send("hello");
         * @endcode
         */
        RPP_CORO_WRAPPER connect_awaiter connect(rpp::socket& sock RPP_LIFETIMEBOUND, const rpp::ipaddress& addr,
                                                 rpp::Duration timeout) noexcept
        {
            return connect_awaiter{ *this, sock, addr, timeout };
        }

        /** @brief Awaiter which waits on the loop thread until a listener has a pending connection, then accepts it. */
        struct RPP_CORO_RETURN_TYPE accept_awaiter : socket_awaiter
        {
            accept_awaiter(event_loop& loop, rpp::socket& listener, rpp::Duration timeout) noexcept
                : socket_awaiter{loop, listener, socket::PF_Read, timeout} {}
            /** @returns the accepted socket, or an invalid one when `timeout` passed */
            rpp::socket await_resume() const noexcept { return ready ? sock.accept(0) : rpp::socket{}; }
        };

        /**
         * @brief Accepts a connection on `listener` on the loop thread, see socket::accept().
         * @returns the accepted socket, or an invalid one when `timeout` passed
         * @code
         *     rpp::socket client = co_await loop.accept(listener, rpp::millis(10));
         * @endcode
         */
        RPP_CORO_WRAPPER accept_awaiter accept(rpp::socket& listener RPP_LIFETIMEBOUND, rpp::Duration timeout) noexcept
        {
            return accept_awaiter{ *this, listener, timeout };
        }

        /**
         * @brief Awaiter that suspends the caller until all forks complete or timeout expires.
         *
         * Event-driven: fork completions resume the joiner directly via post_resume().
         * For timeout: a loop timer resumes the joiner when the forks are still active.
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
                    time_frame frame = loop.get_time_source_frame();
                    rpp::TimePoint deadline = frame.now() + timeout;
                    loop.add_timer(deadline, frame, cont, resume_event{rpp::delegate<void()>{
                        [&loop=loop, cont]() mutable // mutable because of `cont.resume()`
                        {
                            if (loop.fork_joiner == cont)
                            {
                                loop.fork_joiner = {};
                                cont.resume();
                            }
                        }
                    }});
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

        /** @brief A minimal resume-on-main-thread awaiter for coroutines */
        struct RPP_CORO_RETURN_TYPE resume_awaiter
        {
            event_loop& loop;
            bool await_ready() const noexcept { return false; } // never inline
            void await_suspend(rpp::coro_handle<> cont) noexcept { loop.post_resume(cont); }
            void await_resume() const noexcept {}
        };

        /** @brief co_await this to unconditionally reschedule the coroutine onto the loop thread. */
        RPP_CORO_WRAPPER resume_awaiter resume_on_loop() noexcept { return resume_awaiter{ *this }; }

    private:

        // Shared pump used by both run_until_done(task)/(deferred): drive the loop until the
        // already-started task completes, drain the trailing resumes, then return its result.
        template <class Awaitable>
        auto drive_to_done(Awaitable& task) -> decltype(task.await_resume())
        {
            while (!task.done())
                run_once(rpp::millis(5));
            run_all_ready(); // drain callbacks posted during the final resume (e.g. a trailing post())
            return task.await_resume(); // done: returns the value or rethrows (non-blocking)
        }

        struct background_count_guard
        {
            event_loop& loop;
            ~background_count_guard() noexcept
            {
                loop.num_background_suspended.fetch_sub(1, std::memory_order_acq_rel);
            }
        };

        // the decrement runs after the task returns, so no caller can touch the loop past it
        template<class Task>
        void start_in_background(Task&& background_task) noexcept
        {
            num_background_suspended.fetch_add(1, std::memory_order_acq_rel);
            background_pool.parallel_task_detached([this, task = std::forward<Task>(background_task)]() mutable
            {
                background_count_guard guard { *this }; // drops the count even if the task throws
                task();
            });
        }

        // processes a single resume event
        void process_event(resume_event& event) noexcept;

        // invoke the loop hook handler safely, catching any exceptions
        void invoke_loop_hook() noexcept;
    };

} // namespace rpp


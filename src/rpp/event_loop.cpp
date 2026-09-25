/**
 * Single-threaded event loop for serializing coroutine completions.
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "event_loop.h"
#include <climits> // INT_MAX


namespace rpp
{
    // the wall-clock step at which a wait re-reads a warped clock, or a poll with no wake socket re-checks the queue
    static constexpr rpp::Duration POLL_SLICE = rpp::millis(1);

    // rounds a wait up to whole milliseconds for poll(), so a sub-millisecond rest does not spin
    static int poll_millis(rpp::Duration left) noexcept
    {
        if (left <= rpp::Duration::zero())
            return 0;
        const rpp::int64 ms = (left.nsec + NANOS_PER_MILLI - 1) / NANOS_PER_MILLI;
        return ms > INT_MAX ? INT_MAX : int(ms);
    }

    event_loop::event_loop(rpp::uint64 main_thr_id,
                           rpp::thread_pool* background_task_pool,
                           rpp::AtomicTimeSource* warpable_clock) noexcept
        : owner_thread_id{main_thr_id ? main_thr_id : rpp::get_thread_id()}
        , background_pool{background_task_pool ? *background_task_pool : rpp::thread_pool::global()}
        , time_source{warpable_clock}
    {
    }

    event_loop::~event_loop() noexcept
    {
        // if loop is destroyed by anyone other than the owner thread, terminate
        if (!on_owner_thread())
        {
            __assertion_failure("event_loop destroyed from non-owner thread; this is not allowed and may cause resource leaks");
            std::terminate();
        }

        stop();

        // give limited time for cleanup, before asserting an error
        if (!wait_on_all(rpp::millis(2000)))
        {
            // this terminates, except on an MSVC _DEBUG build, where _CrtDbgReport returns
            __assertion_failure("event_loop destroyed with pending tasks; this may cause resource leaks");
        }

        cleanup_forks();

        if (num_forks() > 0)
        {
            __assertion_failure("event_loop destroyed with %d pending forks; cleaning up stale coroutine frames", num_forks());
            // After wait_on_all(), all background threads have finished.
            // The remaining forks are suspended coroutines — safe to destroy.
        }

        // destroy all fork coroutine frames (both completed and stale)
        fork_tasks.clear();

        set_time_source(nullptr); // a worker which outlived the wait must not read a freed clock
    }

    void event_loop::read_time_source(time_frame& frame) const noexcept
    {
        // seq_cst on both sides: either set_time_source() sees this count, or this load
        // sees the new pointer, so this never dereferences a retired clock
        time_source_readers.fetch_add(1, std::memory_order_seq_cst);
        rpp::AtomicTimeSource* src = time_source.load(std::memory_order_seq_cst);
        rpp::uint32 generation = time_source_generation.load(std::memory_order_seq_cst);
        if (src) frame = time_frame{src};
        frame.generation = generation;
        time_source_readers.fetch_sub(1, std::memory_order_release);
    }

    void event_loop::drain_time_source_readers() const noexcept
    {
        // every reader counts, because a reader picks its count before it loads the pointer
        while (time_source_readers.load(std::memory_order_seq_cst) != 0)
            rpp::yield(); // a reader holds the old clock and the owner may free it next
    }

    event_loop::time_frame event_loop::get_time_source_frame() const noexcept
    {
        time_frame frame;
        read_time_source(frame);
        return frame;
    }

    bool event_loop::wait_pop_until(resume_event& event, rpp::TimePoint deadline, time_frame& frame) noexcept
    {
        if (frame.warpable)
        {
            // warpable clock: poll so warp_forward() can release the wait early
            for (;;)
            {
                rpp::Duration left = deadline - current_time(frame);
                if (left <= rpp::Duration::zero())
                    return false;
                if (resume_queue.wait_pop(event, left < POLL_SLICE ? left : POLL_SLICE))
                    return true;
            }
        }
        else
        {
            return resume_queue.wait_pop_until(event, deadline); // wall clock: one efficient wait
        }
    }

    void event_loop::stop() noexcept
    {
        loop_running = false;
        resume_queue.notify_one(); // wake up the loop if it's waiting for events
        poller.wake(); // and when it sits in a socket poll instead
    }

    bool event_loop::wait_on_all(rpp::Duration timeout) noexcept
    {
        // one clock frame for the whole wait, because `end` belongs to it and a drained
        // callback can free the source, see BUGS.md B26
        time_frame frame = get_time_source_frame();
        rpp::TimePoint end = frame.now() + timeout;
        resume_event event;
        while (resume_queue.try_pop(event))
        {
            process_event(event);
        }
        while (has_background_tasks() || pending_waiters() > 0)
        {
            // the count drops with no event behind it, so the wait re-checks it on each slice
            const rpp::TimePoint now = current_time(frame);
            if (now >= end)
                break;
            const rpp::TimePoint slice = now + POLL_SLICE;
            if (wait_next_event(event, slice < end ? slice : end, frame))
            {
                process_event(event);
                invoke_loop_hook();
            }
        }
        return resume_queue.empty() && !has_background_tasks() && pending_waiters() == 0;
    }

    bool event_loop::stop_and_wait_all_ready(rpp::Duration max_wait) noexcept
    {
        // the drain resumes coroutines, which must run on the loop thread
        if (!on_owner_thread())
        {
            LogError("event_loop::stop_and_wait_all_ready() must be called on the event loop thread");
            return false;
        }

        stop();
        wait_on_all(max_wait);
        const bool tasks_done = !has_background_tasks(); // before the drain, so a late worker still counts
        run_all_ready(); // a resume queued as the background count hit zero is still pending
        // a drained resume can start new work, so the detach reads the count again
        // a fork suspended on a post_resume() awaiter counts in neither, so it gets its own term
        const bool idle = !has_pending_work() && num_forks() == 0;
        if (idle)
            set_time_source(nullptr); // a live delay() worker polls it against a virtual deadline
        return tasks_done && idle;
    }

    bool event_loop::run_loop(rpp::Duration suspend_interval) noexcept
    {
        // keep processing until stop() is called and pending tasks are completed
        loop_running = true;
        while (loop_running)
        {
            resume_event event;
            if (wait_next_event(event, suspend_interval))
            {
                process_event(event);
                invoke_loop_hook();
            }
        }

        // expect to finish in reasonable time after stop() is called
        bool all_done = wait_on_all(rpp::millis(500));
        cleanup_forks();
        return all_done;
    }

    bool event_loop::run_once(rpp::Duration timeout) noexcept
    {
        resume_event event;
        const bool got_event = wait_next_event(event, timeout);
        if (got_event)
            process_event(event);

        invoke_loop_hook();
        cleanup_forks();
        return got_event;
    }

    bool event_loop::run_all_ready() noexcept
    {
        bool processed = false;
        resume_event event;
        fire_due_waiters(); // a delay() whose deadline passed is ready too
        while (resume_queue.try_pop(event))
        {
            process_event(event);
            processed = true;
        }

        cleanup_forks();
        return processed;
    }

    int event_loop::run_until_idle(rpp::Duration suspend_interval) noexcept
    {
        int processed_count = 0;
        while (true)
        {
            resume_event event;
            if (wait_next_event(event, suspend_interval))
            {
                process_event(event);
                ++processed_count;
                continue; // loop around one more time just in case
            }

            // invoked between resume events
            invoke_loop_hook();

            // Break only when BOTH: no background tasks AND no queued resume events.
            // If count==0 but queue is non-empty, a task pushed its event just before
            // decrementing - we must process those events before exiting.
            if (!has_pending_work())
                break; // truly idle: no pending tasks and no queued resumes
        }
        cleanup_forks();
        return processed_count;
    }

    void event_loop::run_until_done(event_task& task)
    {
        // when task is done, no more background events should rely on this coroutine,
        // so we can exit the loop
        while (!task.done())
        {
            run_once(rpp::millis(15));
        }

        // drain any callbacks that were posted during the final resume
        // (e.g. post() called right before the coroutine returned)
        // this ignores any other async tasks that were not started by `task` coroutine.
        resume_event event;
        while (resume_queue.try_pop(event))
            process_event(event);

        // propagate the exception
        task.rethrow_if_exception();
    }

    void event_loop::post_resume(rpp::coro_handle<> handle) noexcept
    {
        // the wake runs under the queue lock, so the loop cannot pop the event and die under this thread
        std::unique_lock<rpp::mutex> lock = resume_queue.spin_lock();
        resume_queue.push(lock, resume_event{handle});
        poller.wake();
    }

    bool event_loop::ensure_on_owner_thread(rpp::source_loc loc) const noexcept
    {
        if (owner_thread_id.load(std::memory_order_acquire) == rpp::get_thread_id())
            return true;
        LogErrorFL(loc.file_name(), loc.line(), loc.function_name(),
                   "coroutine not on event_loop owner thread");
        return false;
    }

    void event_loop::post(rpp::delegate<void()>&& callback) noexcept
    {
        // the wake runs under the queue lock, so the loop cannot pop the event and die under this thread
        std::unique_lock<rpp::mutex> lock = resume_queue.spin_lock();
        resume_queue.push(lock, resume_event{std::move(callback)});
        poller.wake();
    }

    void event_loop::notify_fork_joiner() noexcept
    {
        if (num_active_forks.load(std::memory_order_acquire) == 0 && fork_joiner)
        {
            cancel_timers(fork_joiner); // the join deadline is moot once every fork finished
            post_resume(fork_joiner);
            fork_joiner = {};
        }
    }

    bool event_loop::wait_next_event(resume_event& event, rpp::TimePoint deadline, time_frame& frame) noexcept
    {
        for (;;)
        {
            const rpp::Duration left = deadline - current_time(frame);
            const rpp::Duration next = time_to_next_waiter();
            const rpp::Duration wait = next < left ? next : left;
            bool got = false;
            if (wait <= rpp::Duration::zero())
                got = resume_queue.try_pop(event);
            else if (socket_waiters.empty())
                got = wait_pop_until(event, frame.now() + wait, frame);
            else
                got = poll_sockets_until(event, frame.now() + wait, frame);
            fire_due_waiters();
            if (got || resume_queue.try_pop(event))
                return true;
            if (next >= left)
                return false; // the deadline of the caller passed, and no waiter came first
        }
    }

    bool event_loop::wait_next_event(resume_event& event, rpp::Duration timeout) noexcept
    {
        time_frame frame = get_time_source_frame(); // one snapshot per wait, see BUGS.md B26
        return wait_next_event(event, frame.now() + timeout, frame);
    }

    rpp::Duration event_loop::time_to_next_waiter() noexcept
    {
        rpp::Duration next = rpp::Duration::max();
        for (timer& t : timers)
        {
            const rpp::Duration left = t.end - current_time(t.frame);
            if (left < next) next = left;
        }
        for (socket_waiter& w : socket_waiters)
        {
            const rpp::Duration left = w.awaiter->end - current_time(w.awaiter->frame);
            if (left < next) next = left;
        }
        return next;
    }

    void event_loop::fire_due_waiters() noexcept
    {
        rpp::erase_if(timers, [this](timer& t)
        {
            const bool due = current_time(t.frame) >= t.end;
            if (due)
            {
                resume_queue.push(std::move(t.event));
                num_waiters.fetch_sub(1, std::memory_order_acq_rel);
            }
            return due;
        });
        rpp::erase_if(socket_waiters, [this](socket_waiter& w)
        {
            const bool due = current_time(w.awaiter->frame) >= w.awaiter->end;
            if (due) complete_socket_waiter(w, false);
            return due;
        });
    }

    rpp::Duration event_loop::poll_wait(rpp::TimePoint until, time_frame& frame) noexcept
    {
        const rpp::Duration left = until - current_time(frame);
        // a warped clock and a poller without a wake both need short slices, so a warp or a post is seen
        const bool sliced = frame.warpable || !poller.can_wake();
        return sliced && left > POLL_SLICE ? POLL_SLICE : left;
    }

    bool event_loop::poll_sockets_until(resume_event& event, rpp::TimePoint until, time_frame& frame) noexcept
    {
        // 1. a closed socket cannot enter the poll set, so its waiter resumes now and reads the error itself
        for (socket_waiter& w : socket_waiters)
            if (w.awaiter->sock.bad()) complete_socket_waiter(w, true);

        // 2. the poll set follows the order of socket_waiters, so ready(i) belongs to socket_waiters[i]
        poller.clear();
        for (const socket_waiter& w : socket_waiters)
            poller.add(w.awaiter->sock, w.awaiter->flag);

        // 3. the poller arms its wake before the queue check, so a post() after the check still ends the poll
        bool got = false;
        poller.poll(poll_millis(poll_wait(until, frame)), [&]
        {
            got = resume_queue.try_pop(event);
            return got;
        });

        // 4. an error or a hangup marks an entry ready too, and the coroutine reads it from the socket
        for (size_t i = 0; i < socket_waiters.size(); ++i)
            if (poller.ready(int(i))) complete_socket_waiter(socket_waiters[i], true);

        // 5. every waiter which step 1 or step 4 completed leaves the list
        rpp::erase_if(socket_waiters, [](const socket_waiter& w) { return w.awaiter->ready; });

        return got || resume_queue.try_pop(event);
    }

    void event_loop::add_timer(rpp::TimePoint end, time_frame frame, rpp::coro_handle<> owner, resume_event event) noexcept
    {
        run_on_loop([this, end, frame, owner, event = std::move(event)]() mutable
        {
            timers.push_back(timer{end, frame, owner, std::move(event)});
            num_waiters.fetch_add(1, std::memory_order_acq_rel);
        });
    }

    void event_loop::cancel_timers(rpp::coro_handle<> owner) noexcept
    {
        rpp::erase_if(timers, [&](const timer& t)
        {
            const bool cancel = t.owner == owner;
            if (cancel) num_waiters.fetch_sub(1, std::memory_order_acq_rel);
            return cancel;
        });
    }

    void event_loop::add_socket_waiter(socket_awaiter& awaiter, rpp::coro_handle<> cont) noexcept
    {
        run_on_loop([this, &awaiter, cont]
        {
            socket_waiters.push_back(socket_waiter{&awaiter, cont});
            num_waiters.fetch_add(1, std::memory_order_acq_rel);
        });
    }

    void event_loop::complete_socket_waiter(socket_waiter& waiter, bool ready) noexcept
    {
        waiter.awaiter->ready = ready;
        resume_queue.push(resume_event{waiter.cont});
        num_waiters.fetch_sub(1, std::memory_order_acq_rel);
    }

    // automatically clean up completed forks; exceptions go through except_handler
    void event_loop::cleanup_forks() noexcept
    {
        if (fork_tasks.empty())
            return;
        for (auto& task : fork_tasks)
        {
            if (task.done())
            {
                try { task.rethrow_if_exception(); }
                catch (const std::exception& ex)
                {
                    if (except_handler) except_handler(std::current_exception());
                    else LogWarning("event_loop unhandled exception from fork: %s", ex.what());
                }
                catch (...)
                {
                    if (except_handler) except_handler(std::current_exception());
                    else LogWarning("event_loop unhandled exception from fork");
                }
            }
        }
        rpp::erase_if(fork_tasks, [](const event_task& t) { return t.done(); });
    }

    void event_loop::drain_forks()
    {
        if (fork_tasks.empty())
            return;
        for (auto& task : fork_tasks)
        {
            if (task.done())
                task.rethrow_if_exception();
        }
        rpp::erase_if(fork_tasks, [](const event_task& t) { return t.done(); });
    }

    void event_loop::process_event(resume_event& event) noexcept
    {
        if (event.handle)
        {
            try
            {
                event.handle.resume();
            }
            catch (const std::exception& ex)
            {
                if (std_except_handler) std_except_handler(ex);
                else if (except_handler) except_handler(std::current_exception());
                else LogWarning("event_loop unhandled exception from coroutine: %s", ex.what());
            }
            catch (...)
            {
                if (except_handler) except_handler(std::current_exception());
                else LogWarning("event_loop unhandled exception from coroutine");
            }
        }
        else if (event.callback)
        {
            try
            { 
                event.callback();
            }
            catch (const std::exception& ex)
            {
                if (std_except_handler) std_except_handler(ex);
                else if (except_handler) except_handler(std::current_exception());
                else LogError("event_loop unhandled exception from delegate: %s", ex.what());
            }
            catch (...)
            {
                if (except_handler) except_handler(std::current_exception());
                else LogError("event_loop unhandled exception from delegate");
            }
        }
    }

    void event_loop::invoke_loop_hook() noexcept
    {
        if (loop_hook_handler)
        {
            try
            {
                loop_hook_handler();
            }
            catch (const std::exception& ex)
            {
                if (std_except_handler) std_except_handler(ex);
                else if (except_handler) except_handler(std::current_exception());
                else LogError("event_loop hook threw: %s", ex.what());
            }
            catch (...)
            {
                if (except_handler) except_handler(std::current_exception());
                else LogError("event_loop hook threw unknown exception");
            }
        }
    }

} // namespace rpp


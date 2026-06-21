/**
 * Single-threaded event loop for serializing coroutine completions.
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "event_loop.h"

#if RPP_HAS_COROUTINES

namespace rpp
{
    static void loop_post_resume(void* ctx, rpp::coro_handle<> h) noexcept
    {
        static_cast<event_loop*>(ctx)->post_resume(h);
    }

    event_loop::event_loop(rpp::uint64 main_thr_id,
                           rpp::thread_pool* background_task_pool,
                           rpp::AtomicTimeSource* warpable_clock) noexcept
        : owner_thread_id{main_thr_id ? main_thr_id : rpp::get_thread_id()}
        , background_pool{background_task_pool ? *background_task_pool : rpp::thread_pool::global()}
        , time_source{warpable_clock}
    {
        detail::tl_loop = { &loop_post_resume, this };
    }

    event_loop::~event_loop() noexcept
    {
        // if loop is destroyed by anyone other than the owner thread, terminate
        if (rpp::get_thread_id() != owner_thread_id.load(std::memory_order_acquire))
        {
            __assertion_failure("event_loop destroyed from non-owner thread; this is not allowed and may cause resource leaks");
            std::terminate();
        }

        stop();

        // give limited time for cleanup, before asserting an error
        if (!wait_on_all(rpp::millis(1000)))
        {
            __assertion_failure("event_loop destroyed with pending tasks; this may cause resource leaks");
            // do not terminate here (except via DEBUG __assertion_failure), just try to exit gracefully
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

        // Stop advertising this loop on the owner thread's TLS. The constructor registered it so
        // task/cfuture awaiters can post their continuation back here; once we are gone a later
        // await on this thread must not capture a pointer to this destroyed loop (use-after-free).
        if (detail::tl_loop.ctx == this)
            detail::tl_loop = {};
    }

    void event_loop::stop() noexcept
    {
        loop_running = false;
        resume_queue.notify_one(); // wake up the loop if it's waiting for events
    }

    bool event_loop::wait_on_all(rpp::Duration timeout) noexcept
    {
        // drain any remaining events to avoid leaking coroutine frames
        rpp::TimePoint end = rpp::TimePoint::monotonic_now() + timeout;
        resume_event event;
        while (resume_queue.try_pop(event))
        {
            process_event(event);
        }
        // only wait with timeout if there are still background tasks that could post events
        while (has_background_tasks() && resume_queue.wait_pop_until(event, end, time_source))
        {
            process_event(event);
            invoke_loop_hook();
        }
        return resume_queue.empty() && !has_background_tasks();
    }

    bool event_loop::run_loop(rpp::Duration suspend_interval) noexcept
    {
        // keep processing until stop() is called and pending tasks are completed
        loop_running = true;
        while (loop_running)
        {
            resume_event event;
            // background tasks still running: wait for the next resume with a timeout
            // to avoid missing a race where pending_count drops after we checked
            if (resume_queue.wait_pop(event, suspend_interval, time_source))
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
        const bool got_event = timeout == rpp::Duration::zero()
                             ? resume_queue.try_pop(event)
                             : resume_queue.wait_pop(event, timeout, time_source);
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
            // background tasks still running: wait for the next resume with a timeout
            // to avoid missing a race where pending_count drops after we checked
            if (resume_queue.wait_pop(event, suspend_interval, time_source))
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
        resume_queue.push(resume_event{handle});
    }

    void event_loop::post_resume_from_suspension(rpp::coro_handle<> handle) noexcept
    {
        post_resume(handle);
        num_background_suspended.fetch_sub(1, std::memory_order_acq_rel);
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
        resume_queue.push(resume_event{std::move(callback)});
    }

    void event_loop::notify_fork_joiner() noexcept
    {
        if (num_active_forks.load(std::memory_order_acquire) == 0 && fork_joiner)
        {
            post_resume(fork_joiner);
            fork_joiner = {};
        }
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
        auto prev = detail::tl_loop;
        detail::tl_loop = { &loop_post_resume, this };

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

        detail::tl_loop = prev;
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

#endif // RPP_HAS_COROUTINES

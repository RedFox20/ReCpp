/**
 * Single-threaded event loop for serializing coroutine completions.
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "event_loop.h"

#if RPP_HAS_COROUTINES

namespace rpp
{
    event_loop::event_loop(rpp::uint64 main_thr_id,
                           rpp::thread_pool* background_task_pool) noexcept
        : owner_thread_id{main_thr_id ? main_thr_id : rpp::get_thread_id()}
        , background_pool{background_task_pool ? *background_task_pool : rpp::thread_pool::global()}
    {
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
            // do not terminate here, just try to exit gracefully
        }
    }

    void event_loop::stop() noexcept
    {
        loop_running = false;
        resume_queue.notify_one(); // wake up the loop if it's waiting for events
    }

    bool event_loop::wait_on_all(rpp::Duration timeout) noexcept
    {
        // drain any remaining events to avoid leaking coroutine frames
        rpp::TimePoint end = rpp::TimePoint::now() + timeout;
        resume_event event;
        while (resume_queue.wait_pop_until(event, end))
        {
            process_event(event);
        }
        return resume_queue.empty();
    }

    bool event_loop::run_loop() noexcept
    {
        // keep processing until stop() is called and pending tasks are completed
        loop_running = true;
        while (loop_running)
        {
            resume_event event;
            if (resume_queue.try_pop(event))
            {
                process_event(event);
                continue;
            }

            // background tasks still running: wait for the next resume with a timeout
            // to avoid missing a race where pending_count drops after we checked
            if (resume_queue.wait_pop(event, rpp::Duration::from_millis(15)))
            {
                process_event(event);
            }
        }

        // expect to finish in reasonable time after stop() is called
        return wait_on_all(rpp::millis(500));
    }

    bool event_loop::run_once(rpp::Duration timeout) noexcept
    {
        resume_event event;
        if (timeout == rpp::Duration::zero())
        {
            if (!resume_queue.try_pop(event))
                return false;
        }
        else
        {
            if (!resume_queue.wait_pop(event, timeout))
                return false;
        }

        process_event(event);
        return true;
    }

    int event_loop::run_until_idle() noexcept
    {
        int processed_count = 0;
        while (true)
        {
            resume_event event;
            if (resume_queue.try_pop(event))
            {
                process_event(event);
                ++processed_count;
                continue;
            }

            // background tasks still running: wait for the next resume with a timeout
            // to avoid missing a race where pending_count drops after we checked
            if (resume_queue.wait_pop(event, rpp::Duration::from_millis(15)))
            {
                process_event(event);
                ++processed_count;
                continue; // loop around one more time just in case
            }

            if (!has_background_tasks())
                break; // no more pending tasks, we are idle
        }
        return processed_count;
    }

    void event_loop::post_resume(rpp::coro_handle<> handle) noexcept
    {
        resume_queue.push(resume_event{handle});
    }

    void event_loop::post(rpp::delegate<void()> callback) noexcept
    {
        resume_queue.push(resume_event{std::move(callback)});
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
                if (except_handler) except_handler(std::current_exception());
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
                if (except_handler) except_handler(std::current_exception());
                else LogWarning("event_loop unhandled exception from delegate: %s", ex.what());
            }
            catch (...)
            {
                if (except_handler) except_handler(std::current_exception());
                else LogWarning("event_loop unhandled exception from delegate");
            }
        }
    }

} // namespace rpp

#endif // RPP_HAS_COROUTINES

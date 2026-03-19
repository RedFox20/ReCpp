/**
 * Single-threaded event loop for serializing coroutine completions.
 * Copyright (c) 2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "event_loop.h"

#if RPP_HAS_COROUTINES

namespace rpp
{
    event_loop::event_loop() noexcept = default;

    event_loop::~event_loop() noexcept
    {
        // drain any remaining events to avoid leaking coroutine frames
        resume_event event;
        while (resume_queue.try_pop(event))
        {
            process_event(event);
        }
    }

    void event_loop::run_loop()
    {
        owner_thread_id.store(rpp::get_thread_id(), std::memory_order_release);

        // keep processing until no pending work and queue is empty
        for (;;)
        {
            resume_event event;
            if (resume_queue.try_pop(event))
            {
                process_event(event);
                continue;
            }

            // no more events in the queue; check if any background tasks are still running
            if (pending_count.load(std::memory_order_acquire) <= 0)
                break;

            // background tasks still running: wait for the next resume with a timeout
            // to avoid missing a race where pending_count drops after we checked
            if (resume_queue.wait_pop(event, rpp::Duration::from_millis(1)))
            {
                process_event(event);
            }
        }
    }

    bool event_loop::run_once(rpp::Duration timeout)
    {
        owner_thread_id.store(rpp::get_thread_id(), std::memory_order_release);

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

    void event_loop::post_resume(rpp::coro_handle<> handle) noexcept
    {
        resume_queue.push(resume_event{handle});
    }

    void event_loop::post(rpp::delegate<void()> callback) noexcept
    {
        pending_count.fetch_add(1, std::memory_order_acq_rel);
        resume_queue.push(resume_event{std::move(callback)});
    }

    void event_loop::process_event(resume_event& event) noexcept
    {
        if (event.handle)
        {
            pending_count.fetch_sub(1, std::memory_order_acq_rel);
            event.handle.resume();
        }
        else if (event.callback)
        {
            // generic callbacks also decrement pending count
            pending_count.fetch_sub(1, std::memory_order_acq_rel);
            try { event.callback(); }
            catch (...) { /* swallow exceptions from posted callbacks */ }
        }
    }

} // namespace rpp

#endif // RPP_HAS_COROUTINES

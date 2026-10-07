#pragma once
/**
 * A condition variable whose wait sleeps in the kernel on a 32-bit sequence word,
 * through a futex on Linux and Android, and WaitOnAddress() on Windows.
 *
 * Timed waits take rpp::Duration or a monotonic rpp::TimePoint. The Windows wait raises the
 * timer resolution, so a timeout keeps 1ms granularity instead of the ~15.6ms default tick.
 *
 * Copyright (c) 2023-2026, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.types.h" // rpp::int64, rpp::uint32
#include "timepoint.h" // rpp::Duration, rpp::TimePoint
#include "predicates.h" // rpp::IsPredicate
#include "debugging.h" // LogError
#include "mutex.h" // rpp::cvar
#include <atomic>
#include <mutex> // std::unique_lock
#include <type_traits> // std::is_same_v

namespace rpp
{
    /// The result of a timed condition_variable wait without a predicate
    enum class cv_status : rpp::byte
    {
        no_timeout, // a notify or a spurious wakeup ended the wait
        timeout,    // the timeout elapsed
    };

    /**
     * @brief Computes remaining duration from an absolute rpp::TimePoint deadline
     *        using the monotonic clock. Asserts if the remaining time is suspiciously
     *        large (>15s), which usually indicates a clock domain mismatch
     *        (e.g. passing TimePoint::now() instead of TimePoint::monotonic_now()).
     * @returns Remaining duration, clamped to 15ms on bogus values, or 0 if already past.
     */
    inline rpp::Duration _cv_remaining_duration(const rpp::TimePoint& abs_time) noexcept
    {
        rpp::Duration remaining = abs_time - rpp::TimePoint::monotonic_now();
        if (remaining.nsec <= 0)
            return rpp::Duration::zero();
        // a deadline this far away usually comes from TimePoint::now() instead of TimePoint::monotonic_now()
        constexpr rpp::int64 MAX_REASONABLE_NS = 15LL * NANOS_PER_SEC;
        if (remaining.nsec > MAX_REASONABLE_NS)
        {
            LogError("condition_variable::wait_until abs_time is %lld sec in the future,"
                     " possible clock domain mismatch (use TimePoint::monotonic_now())",
                     (long long int)(remaining.nsec / NANOS_PER_SEC));
            return rpp::Duration::from_millis(15); // ~one Win32 tick
        }
        return remaining;
    }

    /**
     * @brief Blocks a thread until a notify, and works with any lock which has lock() and unlock().
     *        A notify changes the sequence word, so a waiter which released its lock never misses it.
     */
    class condition_variable
    {
        static constexpr rpp::uint32 WAITER = 1u << 16; // the high half of state counts the waiters
        static constexpr rpp::uint32 WAKES = WAITER - 1; // the low half counts the wakes which no waiter took yet
        std::atomic_uint32_t seq { 0 }; // the word a waiter sleeps on, and a notify changes
        std::atomic_uint32_t state { 0 }; // lets a notify skip the wake syscall when every waiter has a wake

        // only the exact type, because another lock or a derived mutex can add its own work to lock() and unlock()
        template<class Lock>
        static constexpr bool marks_contended = std::is_same_v<Lock, std::unique_lock<rpp::futex_mutex>>;

        // a waiter reads seq before it registers, so a notify which counts it also changes the seq it sleeps on
        rpp::uint32 begin_wait() noexcept
        {
            const rpp::uint32 s = seq.load(std::memory_order_seq_cst);
            state.fetch_add(WAITER, std::memory_order_seq_cst);
            return s;
        }
        // a waiter takes one wake when it leaves, whichever waiter the kernel woke
        void end_wait() noexcept
        {
            rpp::uint32 s = state.load(std::memory_order_relaxed);
            while (!state.compare_exchange_weak(s, s - WAITER - ((s & WAKES) ? 1 : 0), std::memory_order_relaxed)) {}
        }
        bool begin_notify(bool all) noexcept
        {
            rpp::uint32 s = state.load(std::memory_order_seq_cst);
            for (;;)
            {
                const rpp::uint32 waiters = s / WAITER;
                if ((s & WAKES) >= waiters)
                    return false; // every waiter wakes already, and checks its predicate after the relock
                const rpp::uint32 next = all ? (s & ~WAKES) | waiters : s + 1;
                if (state.compare_exchange_weak(s, next, std::memory_order_seq_cst))
                    break;
            }
            seq.fetch_add(1, std::memory_order_seq_cst);
            return true;
        }

        // relock() takes the mutex without the unique_lock, so the release bypasses it too
        template<class Lock> static void release(Lock& lock) noexcept
        {
            if constexpr (marks_contended<Lock>)
                lock.mutex()->unlock();
            else
                lock.unlock();
        }
        // a woken locker marks the word again only once it runs, so the relock marks it for the lockers still asleep
        template<class Lock> static void relock(Lock& lock) noexcept
        {
            if constexpr (marks_contended<Lock>)
                lock.mutex()->lock_contended();
            else
                lock.lock();
        }

    public:
        condition_variable() noexcept = default;
        condition_variable(const condition_variable&) = delete;
        condition_variable& operator=(const condition_variable&) = delete;

        /// Wakes one waiting thread, if any
        void notify_one() noexcept
        {
            if (begin_notify(false))
                cvar::wake_one(&seq);
        }

        /// Wakes all waiting threads
        void notify_all() noexcept
        {
            if (begin_notify(true))
                cvar::wake_all(&seq);
        }

        /// Releases `lock`, sleeps until a notify, then locks `lock` again
        template<class Lock>
        void wait(Lock& lock) noexcept
        {
            const rpp::uint32 s = begin_wait(); // under the lock, so a notify after the unlock changes seq
            release(lock);
            cvar::wait(&seq, s);
            end_wait();
            relock(lock);
        }

        /// Waits until `stop_waiting()` returns true. It ignores spurious wakeups
        template<class Lock, IsPredicate Predicate>
        void wait(Lock& lock, const Predicate& stop_waiting) noexcept(noexcept(stop_waiting()))
        {
            while (!stop_waiting())
                wait(lock);
        }

        /// Same as wait(lock), with a timeout. @returns cv_status::timeout when `rel_time` elapsed
        template<class Lock>
        [[nodiscard]] cv_status wait_for(Lock& lock, const rpp::Duration& rel_time) noexcept
        {
            const rpp::uint32 s = begin_wait();
            release(lock);
            const bool woken = cvar::wait_for(&seq, s, rel_time);
            end_wait();
            relock(lock);
            return woken ? cv_status::no_timeout : cv_status::timeout;
        }

        /// Same as wait(lock), until a deadline from TimePoint::monotonic_now(). A far deadline reads
        /// as a clock mismatch, see _cv_remaining_duration(). @returns cv_status::timeout at the deadline
        template<class Lock>
        [[nodiscard]] cv_status wait_until(Lock& lock, const rpp::TimePoint& abs_time) noexcept
        {
            rpp::Duration remaining = _cv_remaining_duration(abs_time);
            if (remaining.nsec <= 0)
                return cv_status::timeout;
            return wait_for(lock, remaining);
        }

        /// Waits until `stop_waiting()` returns true, or until the deadline.
        /// @returns the last result of `stop_waiting()`
        template<class Lock, IsPredicate Predicate>
        [[nodiscard]] bool wait_until(Lock& lock, const rpp::TimePoint& abs_time,
                                      const Predicate& stop_waiting) noexcept(noexcept(stop_waiting()))
        {
            while (!stop_waiting())
                if (wait_until(lock, abs_time) == cv_status::timeout)
                    return stop_waiting();
            return true;
        }

        /// Waits until `stop_waiting()` returns true, or until `rel_time` elapsed.
        /// @returns the last result of `stop_waiting()`
        template<class Lock, IsPredicate Predicate>
        [[nodiscard]] bool wait_for(Lock& lock, const rpp::Duration& rel_time,
                                    const Predicate& stop_waiting) noexcept(noexcept(stop_waiting()))
        {
            const rpp::TimePoint deadline = rpp::TimePoint::monotonic_now() + rel_time;
            while (!stop_waiting())
            {
                rpp::Duration remaining = deadline - rpp::TimePoint::monotonic_now();
                if (remaining.nsec <= 0 || wait_for(lock, remaining) == cv_status::timeout)
                    return stop_waiting();
            }
            return true;
        }
    };
}

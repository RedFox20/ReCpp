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
#include <atomic>

namespace rpp
{
    /// Waits in the kernel on a 32-bit word: a futex on Linux and Android, WaitOnAddress() on Windows
    namespace cvar
    {
        /// Sleeps until the 32-bit word at `addr` no longer equals `expected`
        RPPAPI void wait(const void* addr, rpp::uint32 expected) noexcept;

        /// Same as wait(), but at most for `timeout`. @returns false when the timeout elapsed first
        RPPAPI bool wait_for(const void* addr, rpp::uint32 expected, rpp::Duration timeout) noexcept;

        /// Wakes one thread which sleeps on `addr`. It never reads `addr`, so the memory may already be freed
        RPPAPI void wake_one(const void* addr) noexcept;

        /// Wakes every thread which sleeps on `addr`. It never reads `addr`, so the memory may already be freed
        RPPAPI void wake_all(const void* addr) noexcept;
    }

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
        std::atomic_uint32_t seq { 0 }; // the word a waiter sleeps on, and a notify changes
        std::atomic_uint32_t waiters { 0 }; // lets a notify skip the wake syscall when nobody waits

        // a waiter registers under the lock before it reads seq, so a notify which sees no waiter
        // has nobody to wake. A waiter which registers after that check waits after this notify
        rpp::uint32 begin_wait() noexcept
        {
            waiters.fetch_add(1, std::memory_order_seq_cst);
            return seq.load(std::memory_order_seq_cst);
        }
        bool begin_notify() noexcept
        {
            if (waiters.load(std::memory_order_seq_cst) == 0)
                return false;
            seq.fetch_add(1, std::memory_order_seq_cst);
            return true;
        }

    public:
        condition_variable() noexcept = default;
        condition_variable(const condition_variable&) = delete;
        condition_variable& operator=(const condition_variable&) = delete;

        /// Wakes one waiting thread, if any
        void notify_one() noexcept
        {
            if (begin_notify())
                cvar::wake_one(&seq);
        }

        /// Wakes all waiting threads
        void notify_all() noexcept
        {
            if (begin_notify())
                cvar::wake_all(&seq);
        }

        /// Releases `lock`, sleeps until a notify, then locks `lock` again
        template<class Lock>
        void wait(Lock& lock) noexcept
        {
            const rpp::uint32 s = begin_wait(); // under the lock, so a notify after the unlock changes seq
            lock.unlock();
            cvar::wait(&seq, s);
            waiters.fetch_sub(1, std::memory_order_relaxed);
            lock.lock();
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
            lock.unlock();
            const bool woken = cvar::wait_for(&seq, s, rel_time);
            waiters.fetch_sub(1, std::memory_order_relaxed);
            lock.lock();
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

#pragma once
/**
 * This implementation is based on the C++ standard std::condition_variable,
 * and implements Spin-wait timeouts in wait_for() and wait_until(),
 * which allows timeouts smaller than ~15.6ms (default timer tick) on Windows.
 *
 * For UNIX platforms, the standard std::condition_variable is used with
 * rpp::TimePoint adapters
 *
 * Copyright (c) 2023-2025, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
#  include <memory> // shared_ptr
#  include <chrono> // time_point
#  include "mutex.h"
#  include "debugging.h"
#endif

#include "timer.h" // rpp::Duration, rpp::TimePoint
#include "predicates.h" // rpp::IsPredicate
#include <condition_variable> // std::cv_status

namespace rpp
{

#if !_MSC_VER

    // for non-windows platforms use the standard condition variable
    // but with wait() adapters for rpp::TimePoint and rpp::Duration
    //
    // All waits use steady_clock (CLOCK_MONOTONIC) internally to avoid
    // NTP time adjustments causing early/late wakeups. rpp::TimePoint
    // deadlines are converted to remaining durations before waiting.
    class condition_variable : public std::condition_variable
    {
    public:
        using std::condition_variable::condition_variable;

        using clock = std::chrono::steady_clock; // CLOCK_MONOTONIC
        using duration = clock::duration;
        using time_point = clock::time_point;

        /**
         * @brief Blocks the current thread until the condition variable is awakened.
         *
         * @details Atomically releases lock, blocks the current executing thread,
         *          and adds it to the list of threads waiting on *this.
         *          The thread will be unblocked when notify_all() or notify_one() is executed,
         *          or when the relative timeout rel_time expires. It may also be unblocked spuriously.
         *          When unblocked, regardless of the reason, lock is reacquired and wait_for() exits.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param rel_time an object of type rpp::Duration representing the maximum time to spend waiting.
         *
         * @returns std::cv_status::timeout if the relative timeout specified by rel_time expired,
         *          std::cv_status::no_timeout otherwise.
         */
        template<class Mutex>
        [[nodiscard]]
        std::cv_status wait_for(std::unique_lock<Mutex>& lock, const rpp::Duration& rel_time) noexcept
        {
            // use steady_clock (monotonic) to avoid NTP time jumps
            auto steady_deadline = clock::now() + std::chrono::nanoseconds(rel_time.nsec);
            // the wait_until impl will automatically choose MONOTONIC clock
            return std::condition_variable::wait_until(lock, steady_deadline);
        }

        /**
         * @brief Blocks the current thread until the condition variable
         *        is awakened or after the specified timeout duration.
         *
         * @note Internally converts the absolute TimePoint to a remaining duration and
         *       waits using steady_clock (CLOCK_MONOTONIC) to avoid NTP time jump issues.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param abs_time an object of type rpp::TimePoint representing the time when to stop waiting.
         *                 Strongly recommend using TimePoint::monotonic_now() + rel_time to avoid NTP time jump issues.
         * @returns std::cv_status::timeout if the absolute timeout specified
         *          by abs_time was reached, std::cv_status::no_timeout otherwise.
         */
        template<class Mutex>
        [[nodiscard]]
        std::cv_status wait_until(std::unique_lock<Mutex>& lock, const rpp::TimePoint& abs_time) noexcept
        {
            // convert absolute deadline to remaining duration, then wait using steady_clock
            rpp::int64 remaining_ns = abs_time.duration.nsec - rpp::TimePoint::monotonic_now().duration.nsec;
            if (remaining_ns <= 0)
                return std::cv_status::timeout;
            auto steady_deadline = clock::now() + std::chrono::nanoseconds(remaining_ns);
            return std::condition_variable::wait_until(lock, steady_deadline);
        }

        /**
         * @brief This overload may be used to ignore spurious awakenings.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param abs_time an object of type rpp::TimePoint representing the time when to stop waiting
         * @param stop_waiting predicate which returns ​false if the waiting should be continued.
         *
         * @returns false if the predicate pred still evaluates to false
         *          after the abs_time timeout expired, otherwise true.
         */
        template<class Mutex, IsPredicate Predicate>
        [[nodiscard]]
        bool wait_until(std::unique_lock<Mutex>& lock, const rpp::TimePoint& abs_time,
                        const Predicate& stop_waiting) noexcept(std::declval<Predicate>()())
        {
            while (!stop_waiting())
            {
                if (wait_until(lock, abs_time) == std::cv_status::timeout)
                    return stop_waiting();
            }
            return true;
        }

        /**
         * @brief This overload may be used to ignore spurious awakenings
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param rel_time an object of type rpp::Duration representing the maximum time to spend waiting.
         * @param stop_waiting predicate which returns ​false if the waiting should be continued
         *
         * @returns false if the predicate stop_waiting still evaluates
         *          to false after the rel_time timeout expired, otherwise true.
         */
        template<class Mutex, IsPredicate Predicate>
        [[nodiscard]]
        bool wait_for(std::unique_lock<Mutex>& lock, const rpp::Duration& rel_time,
                      const Predicate& stop_waiting) noexcept(std::declval<Predicate>()())
        {
            auto steady_deadline = clock::now() + std::chrono::nanoseconds(rel_time.nsec);
            while (!stop_waiting())
                if (std::condition_variable::wait_until(lock, steady_deadline) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
        }
    };

#else

    /**
     * This implementation is based on the C++ standard std::condition_variable,
     * and implements Spin-wait timeout in wait_for and wait_until, to allow
     * timeouts which are less than 15.6ms on Windows.
     *
     * MSVC++ condition_variable interface implementation was also used as a reference
     */
    class condition_variable final
    {
    public:
        using native_handle_type = void*;

        using clock = std::chrono::steady_clock; // CLOCK_MONOTONIC
        using duration = clock::duration;
        using time_point = clock::time_point;

    private:

        rpp::mutex cs;
        struct { native_handle_type impl; } handle { nullptr };

    public:
        condition_variable() noexcept;
        ~condition_variable() noexcept;

        condition_variable& operator=(condition_variable&&) = delete;
        condition_variable& operator=(const condition_variable&) = delete;

        /** @returns The native handle of the condition variable */
        native_handle_type native_handle() const noexcept { return handle.impl; }

        /**
         * @brief If any threads are waiting on *this, calling notify_one unblocks one of the waiting threads.
         */
        void notify_one() noexcept;

        /**
         * @brief Unblocks all threads currently waiting for *this.
         */
        void notify_all() noexcept;

        /**
         * @brief wait causes the current thread to block until the condition variable is notified
         *
         * @details Atomically unlocks lock, blocks the current executing thread,
         *          and adds it to the list of threads waiting on *this.
         *          The thread will be unblocked when notify_all() or notify_one() is executed.
         *          It may also be unblocked spuriously. When unblocked, regardless of the reason,
         *          lock is reacquired and wait exits.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         */
        template<class Mutex>
        void wait(std::unique_lock<Mutex>& lock) noexcept
        {
            // critical section lock is needed to avoid race condition between notify() and wait()
            // otherwise the notify() may signal just before entering wait() and the signal is lost
            // leading to a deadlocked wait
            cs.lock(); // lock critical section
            lock.unlock(); // unlock the outer lock, and relock when we exit the scope
            (void)_wait_suspended_unlocked(0xFFFFFFFF/*INFINITE*/);
            cs.unlock(); // unlock critical section before relocking outer
            lock.lock(); // relock the outer lock
        }

        /**
         * @brief Blocks the current thread until the condition variable is awakened.
         *
         * @details Atomically releases lock, blocks the current executing thread,
         *          and adds it to the list of threads waiting on *this.
         *          The thread will be unblocked when notify_all() or notify_one() is executed,
         *          or when the relative timeout rel_time expires. It may also be unblocked spuriously.
         *          When unblocked, regardless of the reason, lock is reacquired and wait_for() exits.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param rel_time an object of type std::chrono::duration representing the maximum time to spend waiting.
         *                 Note that rel_time must be small enough not to overflow when added to
         *                 clock::now().
         *
         * @returns std::cv_status::timeout if the relative timeout specified by rel_time expired,
         *          std::cv_status::no_timeout otherwise.
         */
        template<class Mutex>
        [[nodiscard]]
        std::cv_status wait_for(std::unique_lock<Mutex>& lock, const duration& rel_time) noexcept
        {
            const rpp::int64 wait_nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time).count();
            // critical section lock is needed to avoid race condition between notify() and wait()
            // otherwise the notify() may signal just before entering wait() and the signal is lost
            // leading to a deadlocked wait
            cs.lock(); // lock critical section
            lock.unlock(); // unlock the outer lock, and relock when we exit the scope
            std::cv_status status = _wait_for_unlocked(wait_nanos);
            cs.unlock(); // unlock critical section before relocking outer
            lock.lock(); // relock the outer lock
            return status;
        }

        /**
         * @brief This overload may be used to ignore spurious awakenings
         *        while waiting for a specific condition to become true.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param stop_waiting predicate which returns ​false if the waiting should be continued
         */
        template<class Mutex, IsPredicate Predicate>
        void wait(std::unique_lock<Mutex>& lock, const Predicate& stop_waiting) noexcept
        {
            while (!stop_waiting())
                wait(lock);
        }

        /**
         * @brief Blocks the current thread until the condition variable
         *        is awakened or after the specified timeout duration.
         *
         * @details Atomically releases lock, blocks the current executing thread,
         *          and adds it to the list of threads waiting on *this.
         *          The thread will be unblocked when notify_all() or notify_one() is executed,
         *          or when the absolute time point abs_time is reached.
         *          It may also be unblocked spuriously. When unblocked, regardless of the reason,
         *          lock is reacquired and wait_until() exits.
         *          If this function exits via exception, lock is also reacquired.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param abs_time an object of type std::chrono::time_point representing the time when to stop waiting
         * @returns std::cv_status::timeout if the absolute timeout specified
         *          by abs_time was reached, std::cv_status::no_timeout otherwise.
         */
        template<class Mutex>
        [[nodiscard]]
        std::cv_status wait_until(std::unique_lock<Mutex>& lock, const time_point& abs_time) noexcept
        {
            duration rel_time = (abs_time - clock::now());
            return wait_for(lock, rel_time);
        }

        /**
         * @brief This overload may be used to ignore spurious awakenings.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param abs_time an object of type std::chrono::time_point representing the time when to stop waiting
         * @param stop_waiting predicate which returns ​false if the waiting should be continued.
         *
         * @returns false if the predicate pred still evaluates to false
         *          after the abs_time timeout expired, otherwise true.
         */
        template<class Mutex, IsPredicate Predicate>
        [[nodiscard]]
        bool wait_until(std::unique_lock<Mutex>& lock, const time_point& abs_time,
                        const Predicate& stop_waiting) noexcept(std::declval<Predicate>()())
        {
            while (!stop_waiting())
                if (wait_until(lock, abs_time) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
        }

        /**
         * @brief This overload may be used to ignore spurious awakenings
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param rel_time an object of type std::chrono::duration representing the maximum time to spend waiting.
         *                 Note that rel_time must be small enough not to overflow when added to
         *                 clock::now().
         * @param stop_waiting predicate which returns ​false if the waiting should be continued
         *
         * @returns false if the predicate stop_waiting still evaluates
         *          to false after the rel_time timeout expired, otherwise true.
         */
        template<class Mutex, IsPredicate Predicate>
        [[nodiscard]]
        bool wait_for(std::unique_lock<Mutex>& lock, const duration& rel_time,
                      const Predicate& stop_waiting) noexcept(std::declval<Predicate>()())
        {
            auto steady_deadline = clock::now() + rel_time;
            while (!stop_waiting())
                if (wait_until(lock, steady_deadline) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
        }

        ///// rpp::Duration / rpp::TimePoint overloads

        /**
         * @brief Blocks the current thread until the condition variable is awakened.
         *
         * @details Atomically releases lock, blocks the current executing thread,
         *          and adds it to the list of threads waiting on *this.
         *          The thread will be unblocked when notify_all() or notify_one() is executed,
         *          or when the relative timeout rel_time expires. It may also be unblocked spuriously.
         *          When unblocked, regardless of the reason, lock is reacquired and wait_for() exits.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param rel_time an object of type rpp::Duration representing the maximum time to spend waiting.
         *                 Note that rel_time must be small enough not to overflow when added to
         *                 clock::now().
         *
         * @returns std::cv_status::timeout if the relative timeout specified by rel_time expired,
         *          std::cv_status::no_timeout otherwise.
         */
        template<class Mutex>
        [[nodiscard]]
        std::cv_status wait_for(std::unique_lock<Mutex>& lock, const rpp::Duration& rel_time) noexcept
        {
            // critical section lock is needed to avoid race condition between notify() and wait()
            // otherwise the notify() may signal just before entering wait() and the signal is lost
            // leading to a deadlocked wait
            cs.lock(); // lock critical section
            lock.unlock(); // unlock the outer lock, and relock when we exit the scope
            std::cv_status status = _wait_for_unlocked(rel_time.nsec);
            cs.unlock(); // unlock critical section before relocking outer
            lock.lock(); // relock the outer lock
            return status;
        }

        /**
         * @brief Blocks the current thread until the condition variable
         *        is awakened or after the specified timeout duration.
         *
         * @details Atomically releases lock, blocks the current executing thread,
         *          and adds it to the list of threads waiting on *this.
         *          The thread will be unblocked when notify_all() or notify_one() is executed,
         *          or when the absolute time point abs_time is reached.
         *          It may also be unblocked spuriously. When unblocked, regardless of the reason,
         *          lock is reacquired and wait_until() exits.
         *          If this function exits via exception, lock is also reacquired.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param abs_time an object of type rpp::TimePoint (Monotonic) representing the time when to stop waiting
         * @returns std::cv_status::timeout if the absolute timeout specified
         *          by abs_time was reached, std::cv_status::no_timeout otherwise.
         */
        template<class Mutex>
        [[nodiscard]]
        std::cv_status wait_until(std::unique_lock<Mutex>& lock, const rpp::TimePoint& abs_time) noexcept
        {
            rpp::Duration rel_time = (abs_time - rpp::TimePoint::monotonic_now());
            return wait_for(lock, rel_time);
        }

        /**
         * @brief This overload may be used to ignore spurious awakenings.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param abs_time an object of type std::chrono::time_point representing the time when to stop waiting
         *                 Strongly recommend using TimePoint::monotonic_now() + rel_time to avoid NTP time jump issues.
         * @param stop_waiting predicate which returns ​false if the waiting should be continued.
         *
         * @returns false if the predicate pred still evaluates to false
         *          after the abs_time timeout expired, otherwise true.
         */
        template<class Mutex, IsPredicate Predicate>
        [[nodiscard]]
        bool wait_until(std::unique_lock<Mutex>& lock, const rpp::TimePoint& abs_time,
                        const Predicate& stop_waiting) noexcept(std::declval<Predicate>()())
        {
            while (!stop_waiting())
                if (wait_until(lock, abs_time) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
        }

        /**
         * @brief This overload may be used to ignore spurious awakenings
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param rel_time an object of type rpp::Duration representing the maximum time to spend waiting.
         *                 Note that rel_time must be small enough not to overflow when added to
         *                 clock::now().
         * @param stop_waiting predicate which returns ​false if the waiting should be continued
         *
         * @returns false if the predicate stop_waiting still evaluates
         *          to false after the rel_time timeout expired, otherwise true.
         */
        template<class Mutex, IsPredicate Predicate>
        [[nodiscard]]
        bool wait_for(std::unique_lock<Mutex>& lock, const rpp::Duration& rel_time,
                      const Predicate& stop_waiting) noexcept(std::declval<Predicate>()())
        {
            // convert to absolute time, since on GCC that one is faster
            auto steady_deadline = rpp::TimePoint::monotonic_now() + rel_time;
            while (!stop_waiting())
                if (wait_until(lock, steady_deadline) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
        }

    private:

        /**
         * suspended wait which has roughly ~15.6ms tick resolution
         * accuracy on the timeout
         * @returns no_timeout if signaled or error, timeout if timed out without a signal
         */
        std::cv_status _wait_suspended_unlocked(unsigned long timeoutMs) noexcept;

        /**
         * Waits or SpinWaits depending on the desired duration
         * @returns no_timeout if signaled or error, timeout if timed out without a signal
         */
        std::cv_status _wait_for_unlocked(rpp::int64 wait_nanos) noexcept;
    };
#endif // _MSC_VER
}

#pragma once
/**
 * This implementation is based on the C++ standard std::condition_variable,
 * and implements Spin-wait timeouts in wait_for() and wait_until(),
 * which allows timeouts smaller than ~15.6ms (default timer tick) on Windows.
 *
 * For UNIX platforms, the standard std::condition_variable is used
 *
 * Copyright (c) 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
#  include <memory> // shared_ptr
#  include <chrono> // time_point
#  include "mutex.h"
#endif

#include <condition_variable> // std::cv_status

namespace rpp
{

#if !_MSC_VER

    // for non-windows platforms use the standard condition variable
    using condition_variable = std::condition_variable;

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

        // freeze to high_resolution_clock only, because anything else is useless for precise waits
        using clock = std::chrono::high_resolution_clock;
        using duration = clock::duration;
        using time_point = clock::time_point;

    private:
        struct mutex_type;

        struct mutex_type_wrapper
        {
            std::shared_ptr<mutex_type> mtx;
            mutex_type* get() const noexcept { return mtx.get(); }
            void lock() noexcept { _lock(mtx.get()); }
            void unlock() noexcept { _unlock(mtx.get()); }
        };

        std::shared_ptr<mutex_type> mtx;
        struct { native_handle_type impl; } handle { nullptr };
        native_handle_type timer = nullptr; // for high-precision waits

    public:
        condition_variable() noexcept;
        ~condition_variable() noexcept;

        condition_variable& operator=(condition_variable&&) = delete;
        condition_variable& operator=(const condition_variable&) = delete;

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
            mutex_type_wrapper cs {mtx}; // for immunity to *this destruction
            std::unique_lock csGuard {cs}; // critical section
            unlock_guard unlockOuter {lock}; // unlock the outer lock, and relock when we exit the scope

            (void)_wait_suspended_unlocked(cs.get(), 0xFFFFFFFF/*INFINITE*/);

            csGuard.unlock(); // unlock critical section before relocking outer
        }

        /**
         * @brief This overload may be used to ignore spurious awakenings
         *        while waiting for a specific condition to become true.
         *
         * @param lock an object of type std::unique_lock<Mutex>, which must be locked by the current thread
         * @param stop_waiting predicate which returns ​false if the waiting should be continued
         */
        template<class Mutex, class Predicate>
        void wait(std::unique_lock<Mutex>& lock, 
                  const Predicate& stop_waiting) noexcept(stop_waiting())
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
        std::cv_status wait_until(std::unique_lock<Mutex>& lock, 
                                  const time_point& abs_time) noexcept
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
        template<class Mutex, class Predicate>
        [[nodiscard]]
        bool wait_until(std::unique_lock<Mutex>& lock, const time_point& abs_time,
                        const Predicate& stop_waiting) noexcept(stop_waiting())
        {
            while (!stop_waiting())
                if (wait_until(lock, abs_time) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
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
            mutex_type_wrapper cs {mtx}; // for immunity to *this destruction
            std::unique_lock csGuard {cs}; // critical section guard
            unlock_guard unlockOuter {lock}; // unlock the outer lock, and relock when we exit the scope

            std::cv_status status = _wait_for_unlocked(cs.get(), rel_time);

            csGuard.unlock(); // unlock critical section before relocking outer
            return status;
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
        template<class Mutex, class Predicate>
        [[nodiscard]]
        bool wait_for(std::unique_lock<Mutex>& lock, const duration& rel_time,
                      const Predicate& stop_waiting) noexcept(stop_waiting())
        {
            auto abs_time = clock::now() + rel_time;
            while (!stop_waiting())
                if (wait_until(lock, abs_time) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
        }

        // returns the native handle
        native_handle_type native_handle() const noexcept { return handle.impl; }

    private:

        static void _lock(mutex_type* m) noexcept;
        static void _unlock(mutex_type* m) noexcept;

        /**
         * @returns TRUE if CondVar is signaled, FALSE otherwise
         */
        bool _is_signaled(mutex_type* m) noexcept;

        /**
         * suspended wait which has roughly ~15.6ms tick resolution
         * accuracy on the timeout
         * @returns no_timeout if signaled or error, timeout if timed out without a signal
         */
        std::cv_status _wait_suspended_unlocked(mutex_type* m, unsigned long timeoutMs) noexcept;

        /**
         * Waits or SpinWaits depending on the desired duration
         * @returns no_timeout if signaled or error, timeout if timed out without a signal
         */
        std::cv_status _wait_for_unlocked(mutex_type* m, const duration& rel_time) noexcept;
    };
#endif // _MSC_VER
}

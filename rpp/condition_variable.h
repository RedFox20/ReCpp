#pragma once
/**
 * Better cross-platform condition variable, to work around platform specific
 * defects that never get fixed by vendors.
 *
 * Copyright (c) 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
#  include <memory> // shared_ptr
#  include <mutex>  // mutex
#  include <chrono> // time_point
#endif

#include <condition_variable> // std::cv_status

namespace rpp
{
    class condition_variable
        #if !_MSC_VER
            // for non-windows platforms use the regular condition variable
            : public std::condition_variable
        #endif
    {
        // for MSVC, we need to implement everything ourselves because they haven't fixed their bugs for 7+ years
    #if _MSC_VER
    public:
        using native_handle_type = void*;

        // freeze to high_resolution_clock only, because anything else is useless for a proper impl.
        using clock = std::chrono::high_resolution_clock;
        using duration = clock::duration;
        using time_point = clock::time_point;

    private:
        struct mutex_type;
        std::shared_ptr<mutex_type> mtx;
        struct { native_handle_type impl; } handle { nullptr };

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
         * @param lock an object of type std::unique_lock<std::mutex>, which must be locked by the current thread
         */
        void wait(std::unique_lock<std::mutex>& lock) noexcept;

        /**
         * @brief This overload may be used to ignore spurious awakenings
         *        while waiting for a specific condition to become true.
         *
         * @param lock an object of type std::unique_lock<std::mutex>, which must be locked by the current thread
         * @param stop_waiting predicate which returns ​false if the waiting should be continued
         */
        template<class Predicate>
        void wait(std::unique_lock<std::mutex>& lock, 
                  const Predicate& stop_waiting) noexcept(stop_waiting())
        {
            while (!stop_waiting())
                wait(lock);
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
         * @param lock an object of type std::unique_lock<std::mutex>, which must be locked by the current thread
         * @param rel_time an object of type std::chrono::duration representing the maximum time to spend waiting.
         *                 Note that rel_time must be small enough not to overflow when added to
         *                 clock::now().
         *
         * @returns std::cv_status::timeout if the relative timeout specified by rel_time expired,
         *          std::cv_status::no_timeout otherwise.
         */
        [[nodiscard]] 
        std::cv_status wait_for(std::unique_lock<std::mutex>& lock, 
                                const duration& rel_time) noexcept;

        /**
         * @brief This overload may be used to ignore spurious awakenings
         *
         * @param lock an object of type std::unique_lock<std::mutex>, which must be locked by the current thread
         * @param rel_time an object of type std::chrono::duration representing the maximum time to spend waiting.
         *                 Note that rel_time must be small enough not to overflow when added to
         *                 clock::now().
         * @param stop_waiting predicate which returns ​false if the waiting should be continued
         *
         * @returns false if the predicate stop_waiting still evaluates
         *          to false after the rel_time timeout expired, otherwise true.
         */
        template<class Predicate>
        [[nodiscard]]
        bool wait_for(std::unique_lock<std::mutex>& lock, const duration& rel_time,
                      const Predicate& stop_waiting) noexcept(stop_waiting())
        {
            auto abs_time = clock::now() + rel_time;
            while (!stop_waiting())
                if (wait_until(lock, abs_time) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
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
         * @param lock an object of type std::unique_lock<std::mutex>, which must be locked by the current thread
         * @param abs_time an object of type std::chrono::time_point representing the time when to stop waiting
         * @returns std::cv_status::timeout if the absolute timeout specified
         *          by abs_time was reached, std::cv_status::no_timeout otherwise.
         */
        [[nodiscard]]
        std::cv_status wait_until(std::unique_lock<std::mutex>& lock, 
                                  const time_point& abs_time) noexcept;

        /**
         * @brief This overload may be used to ignore spurious awakenings.
         *
         * @param lock an object of type std::unique_lock<std::mutex>, which must be locked by the current thread
         * @param abs_time an object of type std::chrono::time_point representing the time when to stop waiting
         * @param stop_waiting predicate which returns ​false if the waiting should be continued.
         *
         * @returns false if the predicate pred still evaluates to false
         *          after the abs_time timeout expired, otherwise true.
         */
        template<class Predicate>
        [[nodiscard]]
        bool wait_until(std::unique_lock<std::mutex>& lock, const time_point& abs_time,
                        const Predicate& stop_waiting) noexcept(stop_waiting())
        {
            while (!stop_waiting())
                if (wait_until(lock, abs_time) == std::cv_status::timeout)
                    return stop_waiting();
            return true;
        }

        // returns the native handle
        native_handle_type native_handle() noexcept
        {
            return handle.impl;
        }
    #endif
    };
}
#pragma once
#include "condition_variable.h"
#include "debugging.h"
#include "mutex.h"
#include <atomic>
#include <thread> // std::this_thread::yield()

namespace rpp
{
    //////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Simple semaphore for notifying and waiting on events
     */
    class semaphore
    {
    public:
        using mutex_t = rpp::mutex;
        using lock_t = std::unique_lock<mutex_t>;

    protected:
        mutable mutex_t m;
        rpp::condition_variable cv;
        std::atomic_int value{0}; // atomic int to ensure cache coherency
        const int max_value = 0x7FFFFFFF;

    public:
        enum wait_result
        {
            notified, // semaphore was notified
            timeout, // wait timed out
        };

        /**
         * Creates a default semaphore with count = 0 and mav_value = 0x7FFFFFFF
         */
        semaphore() noexcept = default;

        /**
         * @param initialCount Initial semaphore count
         */
        explicit semaphore(int initialCount, int maxCount = 0x7FFFFFFF) noexcept
            : max_value{maxCount}
        {
            reset(initialCount);
        }

        /** @returns Current semaphore count (thread-safe) */
        int count() const noexcept
        {
            auto lock = spin_lock();
            return value;
        }
        int count(lock_t& lock) const noexcept
        {
            (void)lock;
            return value;
        }

        /**
         * @brief Sets the semaphore count to newCount and notifies one waiting thread
         *        if newCount > 0
         */
        void reset(int newCount = 0) noexcept
        {
            auto lock = spin_lock();
            if (0 <= newCount && newCount <= max_value)
                value = newCount;
            if (newCount > 0)
            {
                cv.notify_one();
            }
        }


        /** @brief Returns the internal mutex used by notify() and wait() */
        [[nodiscard]] FINLINE mutex_t& mutex() noexcept { return m; }


        /** @brief Attempts to spin-loop and acquire the internal mutex */
        [[nodiscard]] FINLINE lock_t spin_lock() const noexcept { return rpp::spin_lock(m); }

        /**
         * @brief Increments the semaphore count and notifies one waiting thread
         * 
         * This should be the default preferred way to notify a semaphore
         */
        FINLINE void notify() noexcept
        {
            auto lock = spin_lock();
            notify(lock);
        }
        NOINLINE void notify(lock_t& lock) noexcept
        {
            if (!lock.owns_lock()) LogError("notify(lock) must be called with an owned lock!");
            if (value < 0) LogError("count=%d must not be negative", value.load());
            if (value < max_value) ++value;
            cv.notify_one(); // always notify, to wakeup any waiting threads
        }
        /**
         * @brief Same as notify(), however it also executes a callback function
         *        thread safely just before notifying the waiting thread.
         * This is useful when you need to change some state and then notify a waiting thread.
         */
        template<class Callback> FINLINE void notify(const Callback& callback) noexcept
        {
            auto lock = spin_lock();
            notify<Callback>(lock, callback);
        }
        template<class Callback> FINLINE void notify(lock_t& lock, const Callback& callback) noexcept
        {
            callback(); // <-- perform any state changes here
            notify(lock);
        }


        /**
         * @brief Increments the semaphore count and notifies ALL waiting threads
         * 
         * This should only be used for special cases where all waiting threads need to be notified,
         * it will inherently cause contention issues.
         */
        FINLINE void notify_all() noexcept
        {
            auto lock = spin_lock();
            notify_all(lock);
        }
        NOINLINE void notify_all(lock_t& lock) noexcept
        {
            if (!lock.owns_lock()) LogError("notify_all(lock) must be called with an owned lock!");
            if (value < 0) LogError("count=%d must not be negative", value.load());
            if (value < max_value) ++value;
            cv.notify_all(); // always notify, to wakeup any waiting threads
        }
        /**
         * @brief Same as notify_all(), however it also executes a callback function
         *        thread safely just before notifying the waiting thread.
         * This is useful when you need to change some state and then notify a waiting thread.
         */
        template<class Callback> FINLINE void notify_all(const Callback& callback) noexcept
        {
            auto lock = spin_lock();
            notify_all<Callback>(lock, callback);
        }
        template<class Callback> FINLINE void notify_all(lock_t& lock, const Callback& callback) noexcept
        {
            callback(); // <-- perform any state changes here
            notify_all(lock);
        }


        /**
         * @brief Only notifies one thread if count == 0 (not signaled yet)
         * @returns true if the semaphore was notified
         */
        FINLINE bool notify_once() noexcept
        {
            auto lock = spin_lock();
            return notify_once(lock);
        }
        NOINLINE bool notify_once(lock_t& lock) noexcept
        {
            if (!lock.owns_lock()) LogError("notify_once(lock) must be called with an owned lock!");
            if (value < 0) LogError("count=%d must not be negative", value.load());
            bool shouldNotify = value <= 0;
            if (shouldNotify)
            {
                ++value;
                cv.notify_one();
            }
            return shouldNotify;
        }
        /**
         * @brief Same as notify_once(), however it also executes a callback function
         *        thread safely just before notifying the waiting thread.
         * This is useful when you need to change some state and then notify a waiting thread.
         */
        template<class Callback> FINLINE void notify_once(const Callback& callback) noexcept
        {
            auto lock = spin_lock();
            notify_once<Callback>(lock, callback);
        }
        template<class Callback> FINLINE void notify_once(lock_t& lock, const Callback& callback) noexcept
        {
            callback(); // <-- perform any state changes here
            notify_once(lock);
        }


        /**
         * @brief Tests whether the semaphore is signaled and returns immediately
         * @return true if the semaphore was signaled, false otherwise
         */
        bool try_wait() noexcept
        {
            auto lock = spin_lock();
            if (value > 0)
            {
                --value;
                return true;
            }
            return false;
        }


        /**
         * @brief Waits and loops forever, until the semaphore is signaled, then decrements the count.
         * @warning This can cause a deadlock if the semaphore is never signaled
         */
        FINLINE void wait() noexcept
        {
            auto lock = spin_lock();
            wait(lock);
        }
        void wait(lock_t& lock) noexcept
        {
            wait_no_unset(lock);
            --value; // unset (consume) the value
        }


        /**
         * @brief Waits and loops forever, until the semaphore is signaled, BUT DOES NOT DECREMENT THE COUNT
         * @warning This can cause a deadlock if the semaphore is never signaled
         */
        FINLINE void wait_no_unset() noexcept
        {
            auto lock = spin_lock();
            wait_no_unset(lock);
        }
        NOINLINE void wait_no_unset(lock_t& lock) noexcept
        {
            if (!lock.owns_lock()) LogError("wait(lock) must be called with an owned lock!");
            if (value < 0) LogError("count=%d must not be negative", value.load());
            while (value <= 0) // wait until value is actually set
                cv.wait(lock);
        }


        /**
         * @brief Waits until the semaphore is signaled or the timeout has elapsed, then decrements the count.
         * @param timeout Maximum time to wait for this semaphore to be notified
         * @return signaled if wait was successful or timeout if timeoutSeconds had elapsed
         */
        FINLINE wait_result wait(const rpp::Duration& timeout) noexcept
        {
            auto lock = spin_lock();
            return wait(lock, timeout);
        }
        NOINLINE wait_result wait(lock_t& lock, const rpp::Duration& timeout) noexcept
        {
            auto result = wait_no_unset(lock, timeout);
            if (result == semaphore::notified)
                --value; // unset (consume) the value
            return result;
        }


        /**
         * @brief Waits until the semaphore is signaled, BUT DOES NOT DECREMENT THE COUNT
         *        This is useful if you need a waitable flag that can only be set once.
         * 
         * @param lock Unique lock to use for waiting
         * @param timeout Maximum time to wait for this semaphore to be notified
         * @return signaled if wait was successful or timeout if timeoutSeconds had elapsed
         */
        FINLINE wait_result wait_no_unset(const rpp::Duration& timeout) noexcept
        {
            auto lock = spin_lock();
            return wait_no_unset(lock, timeout);
        }
        NOINLINE wait_result wait_no_unset(lock_t& lock, const rpp::Duration& timeout) noexcept
        {
            if (value < 0) LogError("count=%d must not be negative", value.load());
            if (value <= 0)
            {
                // if timeout is 0, then do not enter this infinite loop, just return instantly
                if (timeout.nsec <= 0)
                    return semaphore::timeout;
                auto until = rpp::TimePoint::now() + timeout;
                while (value <= 0)
                    if (cv.wait_until(lock, until) == std::cv_status::timeout)
                        return semaphore::timeout;
            }
            return semaphore::notified;
        }


        /**
         * Waits while @taskIsRunning is TRUE and sets it to TRUE again before returning
         * This works well for atomic barriers, for example:
         * @code
         *   sync.wait_barrier_while(IsRunning);  // waits while IsRunning == true and sets it to true on return
         *   processTask();
         * @endcode
         * @param taskIsRunning Reference to atomic flag to wait on
         */
        NOINLINE void wait_barrier_while(std::atomic_bool& taskIsRunning) noexcept
        {
            auto lock = spin_lock();
            while (taskIsRunning)
            {
                cv.wait(lock);
            }
            // reset the flag to true
            taskIsRunning = true;
        }

        /**
         * Waits while @atomicFlag is FALSE and sets it to FALSE again before returning
         * This works well for atomic barriers, for example:
         * @code
         *   sync.wait_barrier_until(HasFinished);  // waits while HasFinished == false and sets it to false on return
         *   processResults();
         * @endcode
         * @param hasFinished Reference to atomic flag to wait on
         */
        NOINLINE void wait_barrier_until(std::atomic_bool& hasFinished) noexcept
        {
            auto lock = spin_lock();
            while (!hasFinished)
            {
                cv.wait(lock);
            }
            // reset the flag to false
            hasFinished = false;
        }
    };

    /**
     * @brief A semaphore that can only be set or unset.
     * 
     * notify() - sets the semaphore flag
     * wait() - waits until set, then unsets the semaphore flag
     * wait_no_unset() - waits until set, never unsets
     */
    class semaphore_flag : protected rpp::semaphore
    {
    public:
        semaphore_flag() noexcept : semaphore{0, 1} {}

        /** @returns TRUE if the semaphore is signaled. Does not unset the semaphore count. */
        [[nodiscard]] FINLINE bool is_set() const noexcept { return count() > 0; }
        [[nodiscard]] FINLINE bool is_set(lock_t& lock) const noexcept { return count(lock) > 0; }

        using semaphore::mutex;
        using semaphore::spin_lock;

        using semaphore::notify;
        using semaphore::notify_all;
        using semaphore::notify_once;

        using semaphore::try_wait;
        using semaphore::wait;
        using semaphore::wait_no_unset;
    };

    /**
     * @brief A semaphore that can only be set once and never unset.
     *        This is useful to signal that a run-once task has completed.
     * notify() - sets the semaphore flag and notified ONE listener
     * notify_once() - sets the semaphore flag and notifies ALL listeners
     * wait() - waits until set, but never unsets,
     *          returns immediately if already set
     */
    class semaphore_once_flag : protected rpp::semaphore
    {
    public:
        semaphore_once_flag() noexcept : semaphore{0, 1} {}

        /** @returns TRUE if the semaphore is signaled. Does not unset the semaphore count. */
        [[nodiscard]] FINLINE bool is_set() const noexcept { return count() > 0; }
        [[nodiscard]] FINLINE bool is_set(lock_t& lock) const noexcept { return count(lock) > 0; }

        using semaphore::mutex;
        using semaphore::spin_lock;

        using semaphore::notify;
        using semaphore::notify_all;

        bool try_wait() noexcept
        {
            auto lock = spin_lock();
            return value > 0;
        }

        FINLINE void wait() noexcept
        {
            auto lock = spin_lock();
            wait_no_unset(lock);
        }
        FINLINE void wait(lock_t& lock) noexcept
        {
            wait_no_unset(lock);
        }
        FINLINE wait_result wait(const rpp::Duration& timeout) noexcept
        {
            auto lock = spin_lock();
            return wait_no_unset(lock, timeout);
        }
        FINLINE wait_result wait(lock_t& lock, const rpp::Duration& timeout) noexcept
        {
            return wait_no_unset(lock, timeout);
        }
    };

    //////////////////////////////////////////////////////////////////////////////////////////

    /**
     * @return TRUE if @flag == @expectedValue and atomically sets @flag to @newValue
     */
    inline bool atomic_test_and_set(std::atomic_bool& flag, bool expectedValue = true, bool newValue = false) noexcept
    {
        return flag.compare_exchange_weak(expectedValue, newValue);
    }

    //////////////////////////////////////////////////////////////////////////////////////////
}

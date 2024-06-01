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
        mutable rpp::mutex m;
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
        semaphore() = default;

        /**
         * @param initialCount Initial semaphore count
         */
        explicit semaphore(int initialCount, int maxCount = 0x7FFFFFFF)
            : max_value{maxCount}
        {
            reset(initialCount);
        }

        /** @brief Returns the internal mutex used by notify() and wait() */
        rpp::mutex& mutex() noexcept { return m; }

        /** @returns Current semaphore count (thread-unsafe) */
        int count() const noexcept { return value; }

        /**
         * @brief Sets the semaphore count to newCount and notifies one waiting thread
         *        if newCount > 0
         */
        void reset(int newCount = 0)
        {
            std::lock_guard lock { m };
            if (0 <= newCount && newCount <= max_value)
                value = newCount;
            if (newCount > 0)
            {
                cv.notify_one();
            }
        }

        /**
         * @brief Attempts to spin-loop and acquire the internal mutex
         */
        std::unique_lock<rpp::mutex> spin_lock() const noexcept
        {
            // spin until we can lock the mutex
            if (!m.try_lock())
            {
                for (int i = 0; i < 10; ++i)
                {
                    std::this_thread::yield(); // yielding here will improve perf massively
                    if (m.try_lock())
                        return std::unique_lock{m, std::adopt_lock};
                }

                // suspend until we can lock the mutex
                try {
                    m.lock(); // may throw if deadlock
                } catch (...) {
                    // if we failed to lock, this is most likely a deadlock or the mutex is destroyed
                    // simply give up and return a deferred lock
                    return std::unique_lock{m, std::defer_lock};
                }
            }
            return std::unique_lock{m, std::adopt_lock};
        }

        /**
         * @brief Increments the semaphore count and notifies one waiting thread
         * 
         * This should be the default preferred way to notify a semaphore
         */
        void notify()
        {
            std::lock_guard lock{ m };
            if (value < 0) LogError("count=%d must not be negative", value.load());
            if (value < max_value)
                ++value;
            cv.notify_one(); // always notify, to wakeup any waiting threads
        }

        /**
         * @brief Same as notify(), however it also executes a callback function
         *        thread safely just before notifying the waiting thread.
         * 
         * This is useful when you need to set a flag and then notify a waiting thread.
         */
        template<class Callback>
        void notify(const Callback& callback)
        {
            std::lock_guard lock{ m };
            if (value < 0) LogError("count=%d must not be negative", value.load());
            callback(); // <-- perform any state changes here
            if (value < max_value)
                ++value;
            cv.notify_one(); // always notify, to wakeup any waiting threads
        }

        /**
         * @brief Same as notify(), however the lock is already held by the caller
         */
        void notify(std::unique_lock<rpp::mutex>& lock)
        {
            if (!lock.owns_lock())
            {
                LogError("notify(lock) must be called with an owned lock! locking to avoid a crash");
                lock.lock();
            }
            if (value < 0) LogError("count=%d must not be negative", value.load());
            if (value < max_value)
                ++value;
            cv.notify_one(); // always notify, to wakeup any waiting threads
        }

        /**
         * @brief Increments the semaphore count and notifies ALL waiting threads
         * 
         * This should only be used for special cases where all waiting threads need to be notified,
         * it will inherently cause contention issues.
         */
        void notify_all()
        {
            std::lock_guard lock{ m };
            if (value < 0) LogError("count=%d must not be negative", value.load());
            if (value < max_value)
                value = max_value;
            cv.notify_all(); // always notify, to wakeup any waiting threads
        }

        /**
         * @brief Only notifies one thread if count == 0 (not signaled yet)
         * @returns true if the semaphore was notified
         */
        bool notify_once()
        {
            std::lock_guard lock{ m };
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
         * @brief Tests whether the semaphore is signaled and returns immediately
         * @return true if the semaphore was signaled, false otherwise
         */
        bool try_wait()
        {
            std::lock_guard lock{ m };
            if (value > 0)
            {
                --value;
                return true;
            }
            return false;
        }

        /**
         * @brief Keeps loop waiting until semaphore count is increased
         */
        void wait()
        {
            std::unique_lock lock{ m };
            if (value < 0) LogError("count=%d must not be negative", value.load());
            while (value <= 0) // wait until value is actually set
            {
                cv.wait(lock);
            }
            --value; // consume the value
        }

        /**
         * @param timeout Maximum time to wait for this semaphore to be notified
         * @return signaled if wait was successful or timeout if timeoutSeconds had elapsed
         */
        template<class Rep, class Period>
        wait_result wait(const std::chrono::duration<Rep, Period>& timeout)
        {
            std::unique_lock lock{ m };
            return wait(lock, timeout);
        }

        /**
         * @param lock Unique lock to use for waiting
         * @param timeout Maximum time to wait for this semaphore to be notified
         * @return signaled if wait was successful or timeout if timeoutSeconds had elapsed
         */
        template<class Rep, class Period>
        wait_result wait(std::unique_lock<rpp::mutex>& lock,
                         const std::chrono::duration<Rep, Period>& timeout)
        {
            if (value < 0) LogError("count=%d must not be negative", value.load());
            if (value <= 0)
            {
                auto until = std::chrono::high_resolution_clock::now() + timeout;
                while (value <= 0)
                {
                    if (cv.wait_until(lock, until) == std::cv_status::timeout)
                        return semaphore::timeout;
                }
            }
            --value;
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
        void wait_barrier_while(std::atomic_bool& taskIsRunning)
        {
            std::unique_lock lock{ m };
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
        void wait_barrier_until(std::atomic_bool& hasFinished)
        {
            std::unique_lock lock{ m };
            while (!hasFinished)
            {
                cv.wait(lock);
            }
            // reset the flag to false
            hasFinished = false;
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

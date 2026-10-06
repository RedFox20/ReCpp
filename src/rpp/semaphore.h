#pragma once
#include "condition_variable.h"
#include "debugging.h"
#include "config.types.h" // rpp::__wrap
#include "mutex.h"
#include "timepoint.h" // rpp::Duration
#include "predicates.h" // rpp::IsCallable
#include <atomic>

#include "future_types.h" // rpp::coro_handle
#include "delegate.h" // rpp::delegate (for coroutine awaiter)

namespace rpp
{
    // forward declaration: avoids circular include with thread_pool.h
    void parallel_task_detached(rpp::delegate<void()>&& genericTask) noexcept;

    namespace detail
    {
        /// Awaits a semaphore or a semaphore flag on a pool thread, then resumes the coroutine on that thread
        template<class Waitable, class WaitResult>
        struct RPP_CORO_RETURN_TYPE wait_awaiter
        {
            Waitable& sem;
            rpp::Duration timeout;
            WaitResult result = WaitResult::timeout;

            bool await_ready() noexcept
            {
                if (sem.try_wait()) { result = WaitResult::notified; return true; }
                return false;
            }
            void await_suspend(rpp::coro_handle<> cont) noexcept
            {
                rpp::parallel_task_detached(rpp::delegate<void()>{[this, cont]() mutable
                {
                    result = sem.wait(timeout);
                    cont.resume(); // the coroutine can free this awaiter, so nothing touches it after
                }});
            }
            WaitResult await_resume() noexcept { return result; }
        };
    }

    //////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Simple semaphore for notifying and waiting on events.
     *
     * C++20 coroutine support: use `co_await sem.await(timeout)` to suspend a
     * coroutine until the semaphore is signaled or the timeout expires.
     * Requires `#include <rpp/coroutines.h>` and `using namespace rpp::coro_operators;`.
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
        int count([[maybe_unused]] const lock_t& lock) const noexcept
        {
            return value;
        }

        /**
         * @brief Sets the semaphore count to newCount and notifies one waiting thread
         *        if newCount > 0
         */
        void reset(int newCount = 0) noexcept
        {
            auto lock = spin_lock();
            reset(lock, newCount);
        }
        void reset([[maybe_unused]] const lock_t& lock, int newCount = 0) noexcept
        {
            if (0 <= newCount && newCount <= max_value)
                value = newCount;
            if (newCount > 0)
            {
                cv.notify_one();
            }
        }


        /** @returns the internal mutex used by notify() and wait() */
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
        NOINLINE void notify(const lock_t& lock) noexcept
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
        template<IsCallable Callback>
        FINLINE void notify(const Callback& callback) noexcept
        {
            auto lock = spin_lock();
            notify<Callback>(lock, callback);
        }
        template<IsCallable Callback>
        FINLINE void notify(const lock_t& lock, const Callback& callback) noexcept
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
        NOINLINE void notify_all(const lock_t& lock) noexcept
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
        template<IsCallable Callback>
        FINLINE void notify_all(const Callback& callback) noexcept
        {
            auto lock = spin_lock();
            notify_all<Callback>(lock, callback);
        }
        template<IsCallable Callback>
        FINLINE void notify_all(const lock_t& lock, const Callback& callback) noexcept
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
        NOINLINE bool notify_once(const lock_t& lock) noexcept
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
        template<IsCallable Callback>
        FINLINE void notify_once(const Callback& callback) noexcept
        {
            auto lock = spin_lock();
            notify_once<Callback>(lock, callback);
        }
        template<IsCallable Callback>
        FINLINE void notify_once(const lock_t& lock, const Callback& callback) noexcept
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
            return try_wait(lock);
        }
        bool try_wait([[maybe_unused]] const lock_t& lock) noexcept
        {
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
                auto until = rpp::TimePoint::monotonic_now() + timeout;
                while (value <= 0)
                {
                    // wait_until() treats a far deadline as a clock mismatch, so wait for the time left
                    if (cv.wait_for(lock, until - rpp::TimePoint::monotonic_now()) == rpp::cv_status::timeout)
                    {
                        // recheck value after timeout: a concurrent notify() may have
                        // incremented value between the CV timeout and lock reacquisition
                        return (value > 0) ? semaphore::notified : semaphore::timeout;
                    }
                }
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

        /**
         * @brief Awaitable handle for co_await on semaphore wait.
         * Dispatches the blocking wait to a background thread and resumes the coroutine.
         * @code
         *     auto result = co_await sem.await(rpp::millis(100));
         *     if (result == rpp::semaphore::notified) { // signaled }
         * @endcode
         */
        using co_await_handle = detail::wait_awaiter<semaphore, wait_result>;
        RPP_CORO_WRAPPER co_await_handle await(rpp::Duration timeout) noexcept { return { *this, timeout }; }
    };

    namespace detail
    {
        /// The word of a semaphore flag: bit 0 is the flag, and the bits above count the threads which wait on it
        struct flag_word
        {
            static constexpr rpp::uint32 SET = 1;
            static constexpr rpp::uint32 WAITER = 2;
            std::atomic<rpp::uint32> state { 0 };

            bool is_set() const noexcept { return (state.load(std::memory_order_acquire) & SET) != 0; }

            void unset() noexcept { state.fetch_and(~SET, std::memory_order_relaxed); }

            /// Sets the flag and wakes one waiter, or all. @returns false if the flag was already set
            bool set(bool wake_all) noexcept
            {
                // release pairs with the waiter which sees SET. A waiter can free this flag
                // as soon as it sees SET, so the wake takes the address and reads nothing
                const rpp::uint32 prev = state.fetch_or(SET, std::memory_order_release);
                if (prev >= WAITER)
                {
                    if (wake_all) address_wake_all(&state);
                    else          address_wake_one(&state);
                }
                return (prev & SET) == 0;
            }

            /// @returns true if the flag is set, and clears it when `consume`
            bool try_wait(bool consume) noexcept
            {
                rpp::uint32 s = state.load(std::memory_order_acquire);
                if (!consume)
                    return (s & SET) != 0;
                while (s & SET)
                    if (state.compare_exchange_weak(s, s & ~SET, std::memory_order_acquire, std::memory_order_relaxed))
                        return true;
                return false;
            }

            /// Sleeps until the flag is set, and clears it when `consume`. A null timeout waits forever.
            /// @returns false when the timeout elapsed first
            NOINLINE bool wait(bool consume, const rpp::Duration* timeout) noexcept
            {
                if (try_wait(consume))
                    return true;
                if (timeout && timeout->nsec <= 0)
                    return false;
                const rpp::TimePoint deadline = timeout ? rpp::TimePoint::monotonic_now() + *timeout : rpp::TimePoint{};
                // one RMW word holds the flag and the count, so set() sees this waiter, or this waiter sees SET
                rpp::uint32 s = state.fetch_add(WAITER, std::memory_order_relaxed) + WAITER;
                bool waiting = true;
                for (;;)
                {
                    if ((s & SET) || !waiting)
                    {
                        // leaves as a waiter, and takes the flag when it is set
                        rpp::uint32 next = (s & SET) && consume ? (s - WAITER) & ~SET : s - WAITER;
                        if (state.compare_exchange_weak(s, next, std::memory_order_acquire, std::memory_order_relaxed))
                            return (s & SET) != 0;
                        continue;
                    }
                    if (!timeout)
                    {
                        address_wait(&state, s);
                    }
                    else
                    {
                        rpp::Duration remaining = deadline - rpp::TimePoint::monotonic_now();
                        waiting = remaining.nsec > 0 && address_wait_for(&state, s, remaining);
                    }
                    s = state.load(std::memory_order_relaxed);
                }
            }
        };
    }

    /**
     * @brief A flag which can be set and unset, in one 32-bit word.
     *        A wait sleeps in the kernel on that word, see rpp::condition_variable.
     *
     * notify() - sets the flag and wakes one waiter
     * wait() - waits until set, then unsets the flag
     * wait_no_unset() - waits until set, never unsets
     */
    class semaphore_flag
    {
        detail::flag_word word;

    public:
        using wait_result = rpp::semaphore::wait_result;

        semaphore_flag() noexcept = default;

        /// @returns true if the flag is set. Does not unset it
        [[nodiscard]] bool is_set() const noexcept { return word.is_set(); }

        /// Unsets the flag without a notify
        void unset() noexcept { word.unset(); }

        /// Sets the flag and wakes one waiter if `newCount > 0`, unsets it if `newCount == 0`
        void reset(int newCount = 0) noexcept
        {
            if (newCount > 0) (void)word.set(false);
            else if (newCount == 0) word.unset();
        }

        /// Sets the flag and wakes one waiter
        void notify() noexcept { (void)word.set(false); }

        /// Sets the flag and wakes all waiters, and one of them unsets it
        void notify_all() noexcept { (void)word.set(true); }

        /// Sets the flag and wakes one waiter. @returns false if the flag was already set
        bool notify_once() noexcept { return word.set(false); }

        /// @returns true if the flag was set, and unsets it
        bool try_wait() noexcept { return word.try_wait(true); }

        /// Waits until the flag is set, then unsets it
        void wait() noexcept { (void)word.wait(true, nullptr); }

        /// Waits until the flag is set, then unsets it. @returns timeout if `timeout` elapsed first
        wait_result wait(const rpp::Duration& timeout) noexcept
        {
            return word.wait(true, &timeout) ? semaphore::notified : semaphore::timeout;
        }

        /// Waits until the flag is set, and keeps it set
        void wait_no_unset() noexcept { (void)word.wait(false, nullptr); }

        /// Waits until the flag is set, and keeps it set. @returns timeout if `timeout` elapsed first
        wait_result wait_no_unset(const rpp::Duration& timeout) noexcept
        {
            return word.wait(false, &timeout) ? semaphore::notified : semaphore::timeout;
        }

        /// Awaits the flag on a pool thread, and unsets it, as wait(timeout) does
        using co_await_handle = detail::wait_awaiter<semaphore_flag, wait_result>;
        RPP_CORO_WRAPPER co_await_handle await(rpp::Duration timeout) noexcept { return { *this, timeout }; }
    };

    /**
     * @brief A flag which can only be set once and never unset, in one 32-bit word.
     *        This is useful to signal that a run-once task has completed.
     *
     * notify(), notify_all() - set the flag and wake all waiters
     * wait() - waits until set, never unsets, and returns at once if already set
     */
    class semaphore_once_flag
    {
        detail::flag_word word;

    public:
        using wait_result = rpp::semaphore::wait_result;

        semaphore_once_flag() noexcept = default;

        /// @returns true if the flag is set
        [[nodiscard]] bool is_set() const noexcept { return word.is_set(); }

        /// Sets the flag and wakes all waiters, because the flag stays set for each of them
        void notify() noexcept { (void)word.set(true); }

        /// Sets the flag and wakes all waiters
        void notify_all() noexcept { (void)word.set(true); }

        /// @returns true if the flag is set
        bool try_wait() noexcept { return word.try_wait(false); }

        /// Waits until the flag is set
        void wait() noexcept { (void)word.wait(false, nullptr); }

        /// Waits until the flag is set. @returns timeout if `timeout` elapsed first
        wait_result wait(const rpp::Duration& timeout) noexcept
        {
            return word.wait(false, &timeout) ? semaphore::notified : semaphore::timeout;
        }

        /// Awaits the flag on a pool thread, and keeps it set
        using co_await_handle = detail::wait_awaiter<semaphore_once_flag, wait_result>;
        RPP_CORO_WRAPPER co_await_handle await(rpp::Duration timeout) noexcept { return { *this, timeout }; }
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

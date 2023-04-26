/**
 * Basic concurrent queue with several synchronization helpers, 
 * Copyright (c) 2017-2018, 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#pragma once
#include "config.h"
#include "condition_variable.h"
#include "timer.h"
#include <deque>
#include <mutex>

namespace rpp
{
    /**
     * Provides a simple thread-safe queue with several synchronization helpers
     * as a simple way to write thread-safe code between multiple worker threads.
     * 
     * @note This is not optimized for speed, but has acceptable performance
     *       and due to its simplicity it won't randomly deadlock on you.
     */
    template<class T>
    class concurrent_queue
    {
        std::deque<T> Queue;
        mutable std::mutex Mutex;
        rpp::condition_variable Waiter;

    public:

        using Clock = std::chrono::high_resolution_clock;
        using Duration = Clock::duration;
        using TimePoint = Clock::time_point;

        concurrent_queue() = default;
        ~concurrent_queue() noexcept
        {
            clear(); // safely lock, pop and notify all waiters to give up
        }

        /**
         * @brief Returns the internal mutex for this queue
         */
        [[nodiscard]] std::mutex& sync() const noexcept { return Mutex; }

        /** @returns TRUE if this queue is empty */
        [[nodiscard]] bool empty() const noexcept { return Queue.empty(); }

        /**
         * @returns Approximate size of the queue (unsafe)
         */
        [[nodiscard]] int size() const noexcept { return (int)Queue.size(); }

        /**
         * @returns Synchronized current size of the queue,
         *          however using this value is not atomic
         */
        [[nodiscard]] int safe_size() const noexcept
        {
            if (Mutex.try_lock()) // @note try_lock() for noexcept guarantee
            {
                int sz = size();
                Mutex.unlock();
                return sz;
            }
            return size();
        }

        /**
         * @brief Notifies all waiters that the queue has changed
         */
        void notify() noexcept
        {
            Waiter.notify_all();
        }

        /**
         * @brief Thread-safely clears the entire queue and notifies all waiters
         */
        void clear() noexcept
        {
            if (Mutex.try_lock()) // @note try_lock() for noexcept guarantee
            {
                Queue.clear();
                Mutex.unlock();
                notify();
            }
        }

        /**
         * @returns An atomic copy of the entire queue items
         */
        [[nodiscard]] std::deque<T> atomic_copy() const noexcept
        {
            std::deque<T> copy;
            if (Mutex.try_lock()) // @note try_lock() for noexcept guarantee
            {
                copy = Queue;
                Mutex.unlock();
            }
            return copy;
        }

        /**
         * @brief Thread-safely modify wait condition in the changeWaitFlags callback
         *        and then notifies all waiters.
         *        If any other threads have locked the mutex, then modifying condition
         *        flags could be illegal. This allows synchronizing to the queue state
         *        to ensure all other threads are idle and have released the mutex.
         * @details This is meant to be used with wait_pop() callback overload
         *          where the wait_pop() is checking for a cancellation condition.
         *          This allows for safely setting that condition from another thread
         *          and notify all waiters.
         * @code
         *      queue.notify([this]{ this->atomic_cancelled = true; })
         * @endcode
         */
        template<class ChangeWaitFlags>
        void notify(const ChangeWaitFlags& changeWaitFlags)
        {
            if (Mutex.try_lock()) // @note try_lock() for noexcept guarantee
            {
                changeWaitFlags();
                Mutex.unlock();
                notify();
            }
        }

        /**
         * @brief Thread-safely moves an item into the queue and notifies all waiters
         */
        void push(T&& item)
        {
            {
                std::lock_guard lock {Mutex}; // may throw
                Queue.emplace_back(std::move(item)); // may throw
            }
            notify();
        }

        /**
         * @brief Thread-safely copies an item into the queue and notifies all waiters
         */
        void push(const T& item)
        {
            {
                std::lock_guard lock {Mutex}; // may throw
                Queue.push_back(item); // may throw
            }
            notify();
        }

        /**
         * @brief Thread-safely moves an item into the queue without notifying waiters
         */
        void push_no_notify(T&& item)
        {
            {
                std::lock_guard lock {Mutex}; // may throw
                Queue.emplace_back(std::move(item)); // may throw
            }
        }

        /**
         * @returns Thread-safely pops an item from the queue and notifies other waiters
         * @throws runtime_error if the queue is empty
         */
        [[nodiscard]] T pop()
        {
            T item;
            {
                std::lock_guard lock {Mutex};
                if (Queue.empty()) throw std::runtime_error("concurrent_queue<T>::pop(): Queue was empty!");
                pop_unlocked(item);
            }
            notify();
            return item;
        }

        /**
         * @brief Attempts to pop an item from the queue without waiting
         * @note try_pop() is excellent for polling scenarios if you don't
         *       want to wait for an item, but just check if any work could be done, 
         *       otherwise just continue
         * @returns TRUE if an item was popped, FALSE if the queue was empty
         */ 
        [[nodiscard]] bool try_pop(T& outItem) noexcept
        {
            if (Mutex.try_lock())
            {
                if (Queue.empty())
                {
                    Mutex.unlock();
                    return false;
                }
                pop_unlocked(outItem);
                Mutex.unlock();
                return true;
            }
            return false;
        }

        /**
         * Waits forever until an item is ready to be popped.
         * @warning If no items are present, then this will deadlock!
         * @note This is a convenience method for wait_pop() with no timeout
         *       and is most convenient for producer/consumer threads.
         *       @see test_concurrent_queue.cpp TestCase(basic_producer_consumer) for example usage.
         */
        [[nodiscard]] T wait_pop() noexcept
        {
            std::unique_lock lock {Mutex}; // may throw
            while (Queue.empty())
                Waiter.wait(lock);
            T result;
            pop_unlocked(result);
            return result;
        }

        /**
         * Waits up to @param timeout duration until an item is ready to be popped.
         * For waiting without timeout @see wait_pop().
         * 
         * @note This is best used for cases where you want to wait a specific
         *       amount of time for an item to be available and don't want to
         *       return before that time.
         *       Useful for synchronization tasks that have a time limit.
         * 
         * @param outItem [out] The popped item. Only valid if return value is TRUE
         * @param timeout [required] Time to wait before returning FALSE
         * @return TRUE if an item was popped, FALSE if timed out.
         * @code
         *   string item;
         *   if (queue.wait_pop(item, std::chrono::milliseconds{100}))
         *   {
         *       // item is valid
         *   }
         *   // else: timeout was reached
         * @endcode
         */
        [[nodiscard]]
        bool wait_pop(T& outItem, Duration timeout)
        {
            TimePoint end = Clock::now() + timeout;
            std::unique_lock lock {Mutex}; // may throw
            while (Queue.empty())
            {
                std::cv_status status = Waiter.wait_until(lock, end); // may throw
                if (status == std::cv_status::timeout)
                    break;
            }
            if (Queue.empty())
                return false;
            pop_unlocked(outItem);
            return true;
        }

        /**
         * Waits up to @param timeout duration until an item is ready to be popped.
         * The cancelCondition is used to terminate the wait.
         * 
         * @note This is a wrapper around wait_pop_interval, with the cancellation check
         *       interval set to 1/10th of the timeout.
         * 
         * @param outItem [out] The popped item. Only valid if return value is TRUE
         * @param timeout [required] Maximum time to wait before returning FALSE
         * @param cancelCondition Cancellation condition, CANCEL if cancelCondition()==true
         * @return TRUE if an item was popped,
         *         FALSE if no item popped due to: timeout or cancellation.
         * @code
         *   string item;
         *   auto timeout = std::chrono::milliseconds{100};
         *   if (queue.wait_pop(item, timeout, [this]{ return this->cancelled || this->finished; })
         *   {
         *       // item is valid
         *   }
         * @endcode
         */
        template<class WaitUntil>
        [[nodiscard]]
        bool wait_pop(T& outItem, Duration timeout, const WaitUntil& cancelCondition)
        {
            auto interval = timeout / 10;
            return wait_pop_interval<WaitUntil>(outItem, timeout, interval, cancelCondition);
        }

        /**
         * Waits until an item is ready to be popped.
         * The @param cancelCondition is used to terminate the wait.
         * And @param interval defines how often the cancelCondition is checked.
         * 
         * @note This is a superior alternative to wait_pop(), because the cancelCondition
         *       is checked multiple times, instead of only between waits if someone notifies.
         * 
         * @param outItem [out] The popped item. Only valid if return value is TRUE
         * @param timeout Total timeout for the entire wait loop, granularity is defined by @param interval
         * @param interval Interval for checking the cancellation condition
         * @param cancelCondition Cancellation condition, CANCEL if cancelCondition()==true
         * @return TRUE if an item was popped,
         *         FALSE if no item popped due to: timeout or cancellation
         * @code
         *   string item;
         *   auto interval = std::chrono::milliseconds{100};
         *   if (queue.wait_pop_interval(item, [this]{ return this->cancelled || this->finished; })
         *   {
         *       // item is valid
         *   }
         * @endcode
         */
        template<class WaitUntil>
        [[nodiscard]]
        bool wait_pop_interval(T& outItem, Duration timeout, Duration interval, const WaitUntil& cancelCondition)
        {
            TimePoint next = Clock::now();
            TimePoint end = next + timeout;
            std::unique_lock lock {Mutex};
            while (Queue.empty())
            {
                if (cancelCondition())
                    break;
                next += interval;
                if (next > end)
                    next = end;
                std::cv_status status = Waiter.wait_until(lock, next); // may throw
                if (status == std::cv_status::no_timeout && !Queue.empty())
                    break; // notified
                if (next == end)
                    break; // final timeout
            }
            if (Queue.empty())
                return false;
            pop_unlocked(outItem);
            return true;
        }

    private:
        FINLINE void pop_unlocked(T& outItem) noexcept
        {
            outItem = std::move(Queue.front());
            Queue.pop_front();
        }
    };
}

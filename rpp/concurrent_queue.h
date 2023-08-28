/**
 * Basic concurrent queue with several synchronization helpers, 
 * Copyright (c) 2017-2018, 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#pragma once
#include "condition_variable.h"
#include <vector>
#include <mutex>
#include <thread> // std::this_thread::yield()

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
        int Head = 0; // index of the First element
        int Count = 0; // number of elements in the queue
        int Capacity = 0; // max capacity of the queue
        T* Items = nullptr;

        mutable std::mutex Mutex;
        rpp::condition_variable Waiter;

    public:

        using clock = std::chrono::high_resolution_clock;
        using duration = clock::duration;
        using time_point = clock::time_point;

        concurrent_queue() = default;
        ~concurrent_queue() noexcept
        {
            // safely lock, clear and notify all waiters to give up
            clear();
            if (Items) // all items should be destroyed now, free the ringbuffer
                free(Items);
        }

        concurrent_queue(const concurrent_queue&) = delete;
        concurrent_queue& operator=(const concurrent_queue&) = delete;

        concurrent_queue(concurrent_queue&& q) noexcept
        {
            // safely swap the states from q to default empty state
            std::lock_guard lock { q.Mutex };
            std::swap(Head, q.Head);
            std::swap(Count, q.Count);
            std::swap(Capacity, q.Capacity);
            std::swap(Items, q.Items);
            // notify only the old queue
            q.Waiter.notify_all();
        }

        concurrent_queue& operator=(concurrent_queue&& q) noexcept
        {
            // safely swap the states of both queues
            std::scoped_lock dual_lock { Mutex, q.Mutex };
            std::swap(Head, q.Head);
            std::swap(Count, q.Count);
            std::swap(Capacity, q.Capacity);
            std::swap(Items, q.Items);
            // notify all waiters for both queues
            q.Waiter.notify_all();
            Waiter.notify_all();
            return *this;
        }

        /**
         * @brief Returns the internal mutex for this queue
         */
        [[nodiscard]] std::mutex& sync() const noexcept
        {
            return Mutex;
        }

        /** @returns TRUE if this queue is empty */
        [[nodiscard]] bool empty() const noexcept
        {
            return !Count;
        }

        /**
         * @returns Capacity of the queue (unsafe)
         */
        [[nodiscard]] int capacity() const noexcept
        {
            return Capacity;
        }

        /**
         * @returns Approximate size of the queue (unsafe)
         */
        [[nodiscard]] int size() const noexcept
        {
            return Count;
        }

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
            std::lock_guard lock {Mutex};
            Waiter.notify_all();
        }

        /**
         * @brief Notifies only a single waiter that the queue has changed
         */
        void notify_one() noexcept
        {
            std::lock_guard lock {Mutex};
            Waiter.notify_one();
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
            std::lock_guard lock {Mutex};
            changeWaitFlags();
            Waiter.notify_all();
        }

        /**
         * @brief Thread-safely clears the entire queue and notifies all waiters
         */
        void clear() noexcept
        {
            std::lock_guard lock {Mutex};
            clear_unlocked(); // destroy all elements
            Waiter.notify_all(); // notify all waiters that the queue was emptied
        }

        /**
         * @returns An atomic copy of the entire queue items
         */
        [[nodiscard]] std::vector<T> atomic_copy() const noexcept
        {
            std::vector<T> copy;
            if (Mutex.try_lock()) // @note try_lock() for noexcept guarantee
            {
                int count = Count;
                int capacity = Capacity;
                const T* items = Items;
                copy.reserve(count);
                for (int i = 0, head = Head; i < count; ++i)
                {
                    copy.push_back(items[head++]);
                    if (head == capacity) head = 0; // wrap around the ringbuffer
                }
                Mutex.unlock();
            }
            return copy;
        }

        /**
         * @brief Attempts to pop all pending items from the queue without waiting
         */
        [[nodiscard]] bool try_pop_all(std::vector<T>& outItems) noexcept
        {
            if (empty())
                return false;

            if (Mutex.try_lock())
            {
                if (int count = Count)
                {
                    outItems.resize(count);
                    move_items(Items, Head, count, Capacity, outItems.data());
                    clear_unlocked();
                    Mutex.unlock();
                    return true;
                }
                Mutex.unlock();
                return false;
            }
            else
            {
                // if we failed to lock, yielding here will improve perf by 5-10x
                std::this_thread::yield();
            }
            return false;
        }

        /**
         * @brief Thread-safely moves an item into the queue and notifies one waiter
         */
        void push(T&& item)
        {
            std::lock_guard lock {Mutex}; // may throw
            push_unlocked(std::move(item));
            Waiter.notify_one();
        }

        /**
         * @brief Thread-safely copies an item into the queue and notifies one waiter
         */
        void push(const T& item)
        {
            std::lock_guard lock {Mutex}; // may throw
            push_unlocked(item);
            Waiter.notify_one();
        }

        /**
         * @brief Thread-safely moves an item into the queue without notifying waiters
         */
        void push_no_notify(T&& item)
        {
            std::lock_guard lock {Mutex}; // may throw
            push_unlocked(std::move(item));
        }

        /**
         * @brief Thread-safely copies an item into the queue without notifying waiters
         */
        void push_no_notify(const T& item)
        {
            std::lock_guard lock {Mutex}; // may throw
            push_unlocked(item);
        }

        /**
         * @returns Thread-safely pops an item from the queue and notifies other waiters
         * @throws runtime_error if the queue is empty
         */
        [[nodiscard]] T pop()
        {
            T item;
            std::lock_guard lock {Mutex};
            if (empty())
                throw std::runtime_error{"concurrent_queue<T>::pop(): Queue was empty!"};
            pop_unlocked(item);
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
            if (empty())
                return false;

            if (Mutex.try_lock())
            {
                if (empty())
                {
                    Mutex.unlock();
                    return false;
                }
                pop_unlocked(outItem);
                Mutex.unlock();
                return true;
            }
            else
            {
                // if we failed to lock, yielding here will improve perf by 5-10x
                std::this_thread::yield();
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
            while (empty())
                Waiter.wait(lock);
            T result;
            pop_unlocked(result);
            return result;
        }

        /**
         * Waits up to @param timeout duration until an item is ready to be popped.
         * For waiting without timeout @see wait_pop().
         * 
         * @note This is best used for cases where you want to wait up to a certain time
         *       before giving up. This may return false before the timeout due to spurious wakeups.
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
        bool wait_pop(T& outItem, duration timeout)
        {
            std::unique_lock lock {Mutex}; // may throw
            if (empty())
            {
                #if _MSC_VER // on Win32 wait_for is faster
                    Waiter.wait_for(lock, timeout); // may throw
                #else // on GCC wait_until is faster
                    Waiter.wait_until(lock, clock::now() + timeout); // may throw
                #endif
            }
            if (empty())
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
        bool wait_pop(T& outItem, duration timeout, const WaitUntil& cancelCondition)
        {
            duration interval = timeout / 10;
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
         * @param timeout Total timeout for the entire wait loop, granularity
         *                is defined by @param interval
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
        bool wait_pop_interval(T& outItem, duration timeout, duration interval,
                               const WaitUntil& cancelCondition)
        {
            std::unique_lock lock {Mutex}; // may throw
            if (empty())
            {
                duration remaining = timeout;
                time_point prevTime = clock::now();
                constexpr duration zero = duration{0};
                do
                {
                    if (cancelCondition())
                        return false;
                    #if _MSC_VER // on Win32 wait_for is faster
                        Waiter.wait_for(lock, interval); // may throw
                    #else // on GCC wait_until is faster
                        Waiter.wait_until(lock, prevTime + interval); // may throw
                    #endif
                    if (!empty())
                        break; // got data

                    time_point now = clock::now();
                    remaining -= (now - prevTime);
                    if (remaining <= zero)
                        break; // timed out

                    prevTime = now;
                    // make sure we don't suspend past the final waiting point
                    if (interval > remaining)
                        interval = remaining;
                } while (empty());
            }

            if (empty())
                return false;
            pop_unlocked(outItem);
            return true;
        }

    private:
        inline void push_unlocked(T&& item) noexcept
        {
            int count = Count;
            if (count == Capacity)
                grow();
            new (&Items[(Head + count) % Capacity]) T{ std::move(item) };
            Count = count + 1;
        }
        inline void push_unlocked(const T& item) noexcept
        {
            int count = Count;
            if (count == Capacity)
                grow();
            new (&Items[(Head + count) % Capacity]) T{ item };
            Count = count + 1;
        }
        inline void pop_unlocked(T& outItem) noexcept
        {
            outItem = std::move(Items[Head]);
            if (++Head == Capacity) Head = 0; // wrap around the ringbuffer
            --Count;
        }
        void grow()
        {
            const int oldCap = Capacity;
            int newCapacity = oldCap ? oldCap * 2 : 16;
            T* newItems = (T*)malloc(newCapacity * sizeof(T));
            // reorders the items in the ringbuffer to the beginning of the new ringbuffer
            if (const int count = Count)
            {
                T* oldItems = Items;
                move_items_uninit(oldItems, Head, count, oldCap, newItems);
                free(oldItems);
            }
            Items = newItems;
            Capacity = newCapacity;
            Head = 0;
        }
        inline void move_items_uninit(T* items, int head, int count, int capacity, T* out)
        {
            // move first part of the ringbuffer
            T* oldHead = items + head;
            int firstLen = capacity - head;
            std::uninitialized_move(oldHead, oldHead+firstLen, out);
            // move the second part of the ringbuffer
            if (int secondLen = count - firstLen; secondLen > 0)
                std::uninitialized_move(items, items+secondLen, out+firstLen);
        }
        inline void move_items(T* items, int head, int count, int capacity, T* out)
        {
            // move first part of the ringbuffer
            T* oldHead = items + head;
            int firstLen = capacity - head;
            std::move(oldHead, oldHead+firstLen, out);
            // move the second part of the ringbuffer
            if (int secondLen = count - firstLen; secondLen > 0)
                std::move(items, items+secondLen, out+firstLen);
        }
        inline void copy_items(T* items, int head, int count, int capacity, T* out)
        {
            // copy first part of the ringbuffer
            T* oldHead = items + head;
            int firstLen = capacity - head;
            std::copy(oldHead, oldHead+firstLen, out);
            // move the second part of the ringbuffer
            if (int secondLen = count - firstLen; secondLen > 0)
                std::copy(items, items+secondLen, out+firstLen);
        }
        inline void clear_unlocked() noexcept
        {
            int count = Count;
            int capacity = Capacity;
            const T* items = Items;
            for (int i = 0, head = Head; i < count; ++i)
            {
                items[head++].~T();
                if (head == capacity) head = 0; // wrap around the ringbuffer
            }
            Head = 0;
            Count = 0;
        }
    };
}

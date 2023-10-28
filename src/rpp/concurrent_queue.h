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
#include <type_traits> // std::is_trivially_destructible_v

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
        // for faster performance the Head and Tail are always within the linear range
        // of [ItemsStart, ItemsEnd) and the items are always contiguous in memory.
        T* Head = nullptr;
        T* Tail = nullptr;
        T* ItemsStart = nullptr;
        T* ItemsEnd = nullptr;

        mutable std::mutex Mutex;
        mutable rpp::condition_variable Waiter;

    public:

        using clock = std::chrono::high_resolution_clock;
        using duration = clock::duration;
        using time_point = clock::time_point;

        concurrent_queue() = default;
        ~concurrent_queue() noexcept
        {
            // safely lock, clear and notify all waiters to give up
            clear();
            if (ItemsStart) // all items should be destroyed now, free the ringbuffer
                free(ItemsStart);
        }

        concurrent_queue(const concurrent_queue&) = delete;
        concurrent_queue& operator=(const concurrent_queue&) = delete;

        concurrent_queue(concurrent_queue&& q) noexcept
        {
            {
                // safely swap the states from q to default empty state
                std::lock_guard lock { q.Mutex };
                std::swap(Head, q.Head);
                std::swap(Tail, q.Tail);
                std::swap(ItemsStart, q.ItemsStart);
                std::swap(ItemsEnd, q.ItemsEnd);
            }
            // notify only the old queue
            q.Waiter.notify_all();
        }

        concurrent_queue& operator=(concurrent_queue&& q) noexcept
        {
            {
                // safely swap the states of both queues
                std::scoped_lock dual_lock { Mutex, q.Mutex };
                std::swap(Head, q.Head);
                std::swap(Tail, q.Tail);
                std::swap(ItemsStart, q.ItemsStart);
                std::swap(ItemsEnd, q.ItemsEnd);
            }
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
            return Head == Tail;
        }

        /**
         * @returns Capacity of the queue (unsafe)
         */
        [[nodiscard]] int capacity() const noexcept
        {
            return (int)(ItemsEnd - ItemsStart);
        }

        /**
         * @returns Approximate size of the queue (unsafe)
         */
        [[nodiscard]] int size() const noexcept
        {
            return (int)(Tail - Head);
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
            Waiter.notify_all(); // does not need to be locked
        }

        /**
         * @brief Notifies only a single waiter that the queue has changed
         */
        void notify_one() noexcept
        {
            Waiter.notify_one(); // does not need to be locked
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
            {
                std::unique_lock<std::mutex> lock = spin_lock(); // may throw
                changeWaitFlags();
            }
            Waiter.notify_all();
        }

        /**
         * @brief Thread-safely clears the entire queue and notifies all waiters
         */
        void clear()
        {
            {
                std::unique_lock<std::mutex> lock = spin_lock(); // may throw
                clear_unlocked(); // destroy all elements
            }
            Waiter.notify_all(); // notify all waiters that the queue was emptied
        }

        void reserve(int newCapacity)
        {
            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
            if (newCapacity > capacity())
                grow_to(newCapacity);
        }

        /**
         * @returns An atomic copy of the entire queue items
         */
        [[nodiscard]] std::vector<T> atomic_copy() const noexcept
        {
            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
            return std::vector<T>{Head, Tail};
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
                if (T* head = Head, *tail = Tail; head != tail)
                {
                    outItems.assign(head, tail);
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
            {
                std::unique_lock<std::mutex> lock = spin_lock(); // may throw
                push_unlocked(std::move(item));
            }
            Waiter.notify_one();
        }

        /**
         * @brief Thread-safely copies an item into the queue and notifies one waiter
         */
        void push(const T& item)
        {
            {
                std::unique_lock<std::mutex> lock = spin_lock(); // may throw
                push_unlocked(item);
            }
            Waiter.notify_one();
        }

        /**
         * @brief Thread-safely moves an item into the queue without notifying waiters
         */
        void push_no_notify(T&& item)
        {
            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
            push_unlocked(std::move(item));
        }

        /**
         * @brief Thread-safely copies an item into the queue without notifying waiters
         */
        void push_no_notify(const T& item)
        {
            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
            push_unlocked(item);
        }

        /**
         * @returns Thread-safely pops an item from the queue and notifies other waiters
         * @throws runtime_error if the queue is empty
         */
        [[nodiscard]] T pop()
        {
            T item;
            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
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
         * @brief Attempts to peek an item without popping it. This will copy the item!
         * @returns TRUE if an item was available and was copied to outItem
         */
        [[nodiscard]] bool peek(T& outItem) const noexcept
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
                outItem = *Head;
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
         * @brief Attempts to wait until an item is available.
         * @param timeout Maximum time to wait before returning FALSE
         * @returns TRUE if an item is available to peek or pop
         */
        [[nodiscard]] bool wait_available(duration timeout) const
        {
            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
            wait_notify(lock, timeout); // may throw
            return !empty();
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
            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
            while (empty())
                Waiter.wait(lock); // may throw
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
            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
            if (empty())
            {
                wait_notify(lock, timeout); // may throw
                if (empty())
                    return false;
            }
            pop_unlocked(outItem);
            return true;
        }

        /**
         * Waits up to @param timeout duration until an item is ready and peeks the value
         * without popping it.
         * 
         * @return TRUE if an item was peeked successfully
         */
        [[nodiscard]]
        bool wait_peek(T& outItem, duration timeout) const
        {
            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
            if (empty())
            {
                wait_notify(lock, timeout); // may throw
                if (empty())
                    return false;
            }
            outItem = *Head; // copy (may throw)
            return true;
        }

        /**
         * Only returns items if `clock::now() < until`. This is excellent for message handling
         * loops that have an absolute time limit for processing messages.
         * 
         * @param outItem [out] The popped item. Only valid if return value is TRUE
         * @param until [required] Timepoint to wait until before returning FALSE
         * @return TRUE if an item was popped, FALSE if timed out.
         * @code
         *   auto until = std::chrono::steady_clock::now() + time_limit;
         *   string item;
         *   // process messages until time limit is reached
         *   while (queue.wait_pop_until(item, until))
         *   {
         *       // item is valid
         *   }
         * @endcode
         */
        [[nodiscard]]
        bool wait_pop_until(T& outItem, time_point until)
        {
            auto now = clock::now();

            // if we're already past the time limit, then don't check anything
            // this ensures while() loops don't get stuck processing items endlessly
            if (now > until)
                return false;

            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
            if (empty())
            {
            #if _MSC_VER // on Win32 wait_for is faster
                // don't measure `now` again, even if locking took a while
                // this can make us suspend beyond `until` time, but that's ok
                duration timeout = until - now;
                (void)Waiter.wait_for(lock, timeout); // may throw
            #else // on GCC wait_until is faster
                (void)Waiter.wait_until(lock, until); // may throw
            #endif
                if (empty())
                    return false;
            }
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
            std::unique_lock<std::mutex> lock = spin_lock(); // may throw
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
                    (void)Waiter.wait_for(lock, interval); // may throw
                #else // on GCC wait_until is faster
                    (void)Waiter.wait_until(lock, prevTime + interval); // may throw
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

                if (empty())
                    return false;
            }
            pop_unlocked(outItem);
            return true;
        }

    private:
        std::unique_lock<std::mutex> spin_lock() const
        {
            // spin until we can lock the mutex
            std::unique_lock<std::mutex> lock {Mutex, std::try_to_lock};
            if (!lock.owns_lock())
            {
                for (int i = 0; i < 10; ++i)
                {
                    std::this_thread::yield(); // yielding here will improve perf massively
                    if (lock.try_lock())
                        return lock;
                }

                // suspend until we can lock the mutex
                lock.lock();
            }
            return lock;
        }
        void wait_notify(std::unique_lock<std::mutex>& lock, duration timeout) const
        {
        #if _MSC_VER // on Win32 wait_for is faster
            (void)Waiter.wait_for(lock, timeout); // may throw
        #else // on GCC wait_until is faster
            (void)Waiter.wait_until(lock, clock::now() + timeout); // may throw
        #endif
        }
        void push_unlocked(T&& item) noexcept
        {
            if (Tail == ItemsEnd)
                ensure_capacity();

            if constexpr (std::is_trivially_move_assignable_v<T>)
                *Tail++ = std::move(item);
            else
                new (Tail++) T{ std::move(item) };
        }
        void push_unlocked(const T& item) noexcept
        {
            if (Tail == ItemsEnd)
                ensure_capacity();

            if constexpr (std::is_trivially_copy_assignable_v<T>)
                *Tail++ = item;
            else
                new (Tail++) T{ item };
        }
        void pop_unlocked(T& outItem) noexcept
        {
            T* head = Head++;
            outItem = std::move(*head);
            if constexpr (!std::is_trivially_destructible_v<T>)
                head->~T();

            if (Head == Tail) // clear the queue if Head catches the Tail
            {
                clear_unlocked();
            }
        }
        void ensure_capacity() noexcept
        {
            const int oldCap = capacity();
            // we have enough capacity, just shift the items to the front
            if (oldCap > 0 && (Head - ItemsStart) >= (oldCap / 2))
            {
                // unshift elements to the front of the queue
                T* newStart = ItemsStart;
                T* newTail = move_items(Head, Tail, newStart);
                Head = newStart;
                Tail = newTail;
            }
            else // grow
            {
                int growBy = oldCap ? oldCap : 32;
                if (growBy > (16*1024)) growBy = 16*1024;
                const int newCap = oldCap + growBy;
                grow_to(newCap);
            }
        }
        void grow_to(int newCap) noexcept
        {
            T* oldStart = ItemsStart;
            T* newStart = (T*)malloc(newCap * sizeof(T));
            T* newTail = move_items(Head, Tail, newStart);
            Head = newStart;
            Tail = newTail;
            ItemsStart = newStart;
            ItemsEnd = newStart + newCap;
            free(oldStart);
        }
        static T* move_items(T* oldHead, T* oldTail, T* newStart) noexcept
        {
            if constexpr (std::is_trivially_move_assignable_v<T>)
            {
                int count = (oldTail - oldHead);
                memmove(newStart, oldHead, count * sizeof(T));
                return newStart + count;
            }
            else
            {
                T* newTail = newStart;
                for (; oldHead != oldTail; ++newTail, ++oldHead)
                {
                    new (newTail) T{ std::move(*oldHead) };
                    if constexpr (!std::is_trivially_destructible_v<T>)
                        oldHead->~T();
                }
                return newTail;
            }
        }
        void clear_unlocked() noexcept
        {
            for (T* head = Head, *tail = Tail; head != tail; ++head)
                head->~T();

            // if the capacity was huge, then free the entire buffer
            // to avoid massive memory usage for a small queue
            if (capacity() > 8192)
            {
                free(ItemsStart);
                ItemsStart = ItemsEnd = nullptr;
            }

            Head = Tail = ItemsStart;
        }
    };
}

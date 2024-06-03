/**
 * Basic concurrent queue with several synchronization helpers, 
 * Copyright (c) 2017-2018, 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#pragma once
#include "config.h"
#include "semaphore.h"
#include <vector>
#include <thread> // std::this_thread::yield()
#include <type_traits> // std::is_trivially_destructible_v
#include <optional> // std::optional
#include <atomic> // std::atomic_bool
#include <cstring> // memmove

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
        T* ItemsStart = nullptr; // storage for the items
        T* ItemsEnd = nullptr;

    public:
        using clock = std::chrono::high_resolution_clock;
        using duration = clock::duration;
        using time_point = clock::time_point;

    private:
        using mutex_t = rpp::mutex;
        using lock_t = rpp::semaphore::lock_t;
        mutable mutex_t Mutex;
        mutable rpp::condition_variable Waiter;
        // special state flag for all waiters to immediately exit the queue
        std::atomic_bool Cleared = false;

    public:
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
            // safely swap the states from q to default empty state
            std::lock_guard lock { q.mutex() };
            std::swap(Head, q.Head);
            std::swap(Tail, q.Tail);
            std::swap(ItemsStart, q.ItemsStart);
            std::swap(ItemsEnd, q.ItemsEnd);
            // notify only the old queue
            q.notify_all_unlocked();
        }

        concurrent_queue& operator=(concurrent_queue&& q) noexcept
        {
            // safely swap the states of both queues
            std::scoped_lock dual_lock { mutex(), q.mutex() };
            std::swap(Head, q.Head);
            std::swap(Tail, q.Tail);
            std::swap(ItemsStart, q.ItemsStart);
            std::swap(ItemsEnd, q.ItemsEnd);
            // notify all waiters for both queues
            q.notify_all_unlocked();
            notify_all_unlocked();
            return *this;
        }

        /**
         * @brief Returns the internal mutex for this queue
         */
        [[nodiscard]] FINLINE mutex_t& mutex() const noexcept
        {
            return Mutex;
        }

        [[nodiscard]] FINLINE lock_t spin_lock() const noexcept
        {
            return rpp::spin_lock(Mutex);
        }

        /** @returns TRUE if this queue is empty */
        [[nodiscard]] FINLINE bool empty() const noexcept
        {
            auto lock = spin_lock();
            return Head == Tail;
        }

        /**
         * @returns Capacity of the queue (unsafe)
         */
        [[nodiscard]] FINLINE size_t capacity() const noexcept
        {
            return (ItemsEnd - ItemsStart);
        }

        /**
         * @returns Approximate size of the queue (unsafe)
         */
        [[nodiscard]] FINLINE size_t size() const noexcept
        {
            return (Tail - Head);
        }

        /**
         * @returns Synchronized current size of the queue,
         *          however using this value is not atomic
         */
        [[nodiscard]] size_t safe_size() const noexcept
        {
            if (mutex().try_lock()) // @note try_lock() for noexcept guarantee
            {
                size_t sz = size();
                mutex().unlock();
                return sz;
            }
            return size();
        }

        /**
         * @brief Notifies all waiters that the queue has changed
         */
        void notify() noexcept
        {
            auto lock = spin_lock();
            notify_all_unlocked();
        }

        /**
         * @brief Notifies only a single waiter that the queue has changed
         */
        void notify_one() noexcept
        {
            auto lock = spin_lock();
            notify_one_unlocked();
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
        void notify(const ChangeWaitFlags& changeWaitFlags) noexcept(noexcept(changeWaitFlags()))
        {
            auto lock = spin_lock();
            changeWaitFlags();
            notify_all_unlocked();
        }

        /**
         * @brief Thread-safely clears the entire queue and notifies all waiters
         */
        void clear() noexcept
        {
            auto lock = spin_lock();
            clear_unlocked(); // destroy all elements
            Cleared = true; // set the cleared flag
            notify_all_unlocked(); // notify all waiters that the queue was emptied
        }

        void reserve(int newCapacity) noexcept
        {
            auto lock = spin_lock();
            if (newCapacity > capacity())
                grow_to(newCapacity);
        }

        /**
         * @returns An atomic copy of the entire queue items
         */
        [[nodiscard]] std::vector<T> atomic_copy() const noexcept
        {
            auto lock = spin_lock();
            return std::vector<T>{Head, Tail};
        }

        /** @brief A no-op proxy for iterating an already locked queue */
        struct iterator_proxy
        {
            concurrent_queue* Queue;
            [[nodiscard]] FINLINE T* begin() noexcept { return Queue->Head; }
            [[nodiscard]] FINLINE T* end()   noexcept { return Queue->Tail; }
            [[nodiscard]] FINLINE const T* begin() const noexcept { return Queue->Head; }
            [[nodiscard]] FINLINE const T* end()   const noexcept { return Queue->Tail; }
        };

        /** @brief A safe iterator for the queue - which holds a lock until iteration is complete */
        struct iterator_lock : public iterator_proxy
        {
            lock_t Lock;
            [[nodiscard]] FINLINE lock_t& lock() noexcept { return Lock; }
            [[nodiscard]] FINLINE T* erase(T* it) noexcept { return this->Queue->erase(Lock, it); }
        };

        /** @brief creates a safe iterator for the queue */
        FINLINE       iterator_lock iterator()       noexcept { return { this, spin_lock() }; }
        FINLINE const iterator_lock iterator() const noexcept { return { this, spin_lock() }; }

        /** @brief Creates a safe iterator for the queue, using a previously acquired lock */
        FINLINE       iterator_proxy iterator(lock_t& lock)       noexcept { if (!lock.owns_lock()) { lock.lock(); } return { this }; }
        FINLINE const iterator_proxy iterator(lock_t& lock) const noexcept { if (!lock.owns_lock()) { lock.lock(); } return { this }; }

        /**
         * @brief Erases an item from the queue and shifts all items after it
         * @param lock Mandatory queue.mutex() lock to ensure iteration is thread-safe
         * @param it The item to erase
         * @returns Iterator position of the next item after the erased item
         *          (behavior identical to std::vector::erase())
         */
        T* erase(lock_t& lock, T* it) noexcept
        {
            // if the lock is not held, or the iterator is out of bounds, then return the end
            if (!lock.owns_lock() || it < Head || it >= Tail)
                return Tail;

            if constexpr (!std::is_trivially_destructible_v<T>)
                it->~T();

            // shift all items after the erased item
            for (T* i = it + 1; i < Tail; ++i)
                *(i - 1) = std::move(*i);

            --Tail;
            return it;
        }

        /**
         * @brief Attempts to pop all pending items from the queue without waiting
         */
        [[nodiscard]] bool try_pop_all(std::vector<T>& outItems) noexcept
        {
            if (Head == Tail)
                return false;

            if (mutex().try_lock())
            {
                if (T* head = Head, *tail = Tail; head != tail)
                {
                    outItems.assign(head, tail);
                    clear_unlocked();
                    mutex().unlock();
                    return true;
                }
                mutex().unlock();
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
        void push(T&& item) noexcept
        {
            auto lock = spin_lock();
            push_unlocked(std::move(item));
            notify_one_unlocked();
        }

        /**
         * @brief Thread-safely copies an item into the queue and notifies one waiter
         */
        void push(const T& item) noexcept
        {
            auto lock = spin_lock();
            push_unlocked(item);
            notify_one_unlocked();
        }

        /**
         * @brief Thread-safely moves an item into the queue without notifying waiters
         */
        void push_no_notify(T&& item) noexcept
        {
            auto lock = spin_lock();
            push_unlocked(std::move(item));
        }

        /**
         * @brief Thread-safely copies an item into the queue without notifying waiters
         */
        void push_no_notify(const T& item) noexcept
        {
            auto lock = spin_lock();
            push_unlocked(item);
        }

        /**
         * @returns Thread-safely pops an item from the queue and notifies other waiters
         * @throws runtime_error if the queue is empty
         */
        [[nodiscard]] T pop()
        {
            T item;
            auto lock = spin_lock();
            if (Head == Tail)
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
            if (Head == Tail)
                return false;

            if (mutex().try_lock())
            {
                if (Head == Tail)
                {
                    mutex().unlock();
                    return false;
                }
                pop_unlocked(outItem);
                mutex().unlock();
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
            if (Head == Tail)
                return false;

            if (mutex().try_lock())
            {
                if (Head == Tail)
                {
                    mutex().unlock();
                    return false;
                }
                outItem = *Head;
                mutex().unlock();
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
         * @brief Moves the front item without popping it. This will
         *        allow for proper atomic operations where the item is not removed
         *        until it has been fully processed.
         * @code
         * T item;
         * if (queue.pop_atomic_start(item))
         * {
         *     channel.send(item); // process the item (slow)
         *     queue.pop_atomic_end(); // remove the processed item from the queue
         * }
         * @endcode
         * @param outItem [out] The front item. Only valid if return value is TRUE
         * @returns true if an item was moved to outItem
         */
        [[nodiscard]] bool pop_atomic_start(T& outItem) noexcept
        {
            if (Head == Tail)
                return false;

            if (mutex().try_lock())
            {
                if (Head == Tail)
                {
                    mutex().unlock();
                    return false;
                }
                outItem = std::move(*Head);
                mutex().unlock();
                return true;
            }
            else
            {
                // if we failed to lock, yielding here will improve perf by 5-10x
                std::this_thread::yield();
            }
            return false;
        }

        /** @see pop_atomic_start() */
        void pop_atomic_end() noexcept
        {
            auto lock = spin_lock();
            if (Head != Tail)
                pop_unlocked();
        }

        /**
         * @brief Atomically pops and processes an item within a callback.
         *        The item is removed only if the callback returns TRUE.
         * @see pop_atomic_start() and pop_atomic_end()
         * @param callback [in] The callback to process the item, taking 1 argument which Item &&
         * @returns TRUE if an item was popped and processed
         */
        template<class Lambda> FINLINE bool pop_atomic(const Lambda& callback) noexcept
        {
            T item;
            if (pop_atomic_start(item))
            {
                callback(std::move(item));
                pop_atomic_end();
                return true;
            }
            return false;
        }

        /**
         * @brief Attempts to wait until an item is available.
         * @returns TRUE if an item is available to peek or pop
         */
        [[nodiscard]] bool wait_available() const noexcept
        {
            auto lock = spin_lock();
            return wait_notify(lock);
        }

        /**
         * @brief Attempts to wait until an item is available.
         * @param timeout Maximum time to wait before returning FALSE
         * @returns TRUE if an item is available to peek or pop
         */
        [[nodiscard]] bool wait_available(duration timeout) const noexcept
        {
            auto lock = spin_lock();
            return wait_notify_for(lock, timeout);
        }

        /**
         * Waits until an item is available, or until this queue is NOTIFIED
         * @note This is a convenience method for wait_pop() with no timeout
         *       and is most convenient for producer/consumer threads.
         *       @see test_concurrent_queue.cpp TestCase(basic_producer_consumer) for example usage.
         * @returns The popped item, or std::nullopt if the queue had no items when NOTIFIED
         */
        [[nodiscard]] std::optional<T> wait_pop() noexcept
        {
            auto lock = spin_lock();
            if (!wait_notify(lock))
                return std::nullopt;
            T result;
            pop_unlocked(result);
            return result;
        }

        /**
         * Waits until an item is available, or until this queue is NOTIFIED
         * @note This variant does not use a timeout and thus has better performance.
         *       However, this means the queue needs to be notified to wake up
         */
        [[nodiscard]]
        bool wait_pop(T& outItem) noexcept
        {
            auto lock = spin_lock();
            if (!wait_notify(lock))
                return false;
            pop_unlocked(outItem);
            return true;
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
        bool wait_pop(T& outItem, duration timeout) noexcept
        {
            auto lock = spin_lock();
            if (!wait_notify_for(lock, timeout))
                return false;
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
        bool wait_peek(T& outItem, duration timeout) const noexcept
        {
            auto lock = spin_lock();
            if (!wait_notify_for(lock, timeout))
                return false;
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
        bool wait_pop_until(T& outItem, time_point until) noexcept
        {
            auto now = clock::now();

            // if we're already past the time limit, then don't check anything
            // this ensures while() loops don't get stuck processing items endlessly
            if (now > until)
                return false;

            auto lock = spin_lock();
            if (!wait_notify_until(lock, until))
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
        bool wait_pop(T& outItem, duration timeout, const WaitUntil& cancelCondition)  noexcept(noexcept(cancelCondition()))
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
         *  NOTE: this is just a hint and there is no guarantee to make this accurate without
         *        affecting system performance. A 1ms interval could be anywhere from 1ms to 15ms.
         * @param cancelCondition Cancellation condition, CANCEL if cancelCondition()==true
         * @return TRUE if an item was popped,
         *         FALSE if no item popped due to: timeout or cancellation
         * @code
         *   string item;
         *   auto interval = std::chrono::milliseconds{100};
         *   if (queue.wait_pop_interval(item, timeout, interval, [this]{ return this->cancelled || this->finished; })
         *   {
         *       // item is valid
         *   }
         * @endcode
         */
        template<class WaitUntil>
        [[nodiscard]]
        bool wait_pop_interval(T& outItem, duration timeout, duration interval,
                               const WaitUntil& cancelCondition) noexcept(noexcept(cancelCondition()))
        {
            auto lock = spin_lock();
            if (Head == Tail)
            {
                duration remaining = timeout;
                time_point prevTime = clock::now();
                constexpr duration zero = duration{0};
                do
                {
                    if (cancelCondition())
                        return false;
                #if _MSC_VER // on Win32 wait_for is faster
                    (void)Waiter.wait_for(lock, interval); // noexcept
                #else // on GCC wait_until is faster
                    (void)Waiter.wait_until(lock, prevTime + interval); // may throw
                #endif
                    if (Head != Tail) break; // got data
                    if (Cleared) return false; // give up immediately

                    time_point now = clock::now();
                    remaining -= (now - prevTime);
                    if (remaining <= zero)
                        break; // timed out

                    prevTime = now;
                    // make sure we don't suspend past the final waiting point
                    if (interval > remaining)
                        interval = remaining;
                } while (Head == Tail);

                if (Head == Tail)
                    return false;
            }
            pop_unlocked(outItem);
            return true;
        }

    private:
        // waits until any wakeup signal and returns true if there is an item
        bool wait_notify(lock_t& lock) const noexcept
        {
            if (Head != Tail) return true; // wait is not needed
            (void)Waiter.wait(lock); // noexcept
            return Head != Tail;
        }
        // wait_notify with a timeout, returns true if there is an item
        bool wait_notify_for(lock_t& lock, const duration& timeout) const noexcept
        {
            if (Head != Tail) return true; // wait is not needed
            auto now = clock::now();
            auto end = now + timeout;
            do {
                #if _MSC_VER // on Win32 wait_for is faster
                    duration remaining = end - now;
                    (void)Waiter.wait_for(lock, remaining); // noexcept
                #else // on GCC wait_until is faster
                    (void)Waiter.wait_until(lock, end); // may throw
                #endif
                if (Head != Tail) return true; // got an item
                if (Cleared) return false; // give up immediately
                now = clock::now();
            } while (now < end); // handle spurious wakeups
            return false;
        }
        bool wait_notify_until(lock_t& lock, const time_point& until) const noexcept
        {
            if (Head != Tail) return true; // wait is not needed
            time_point now;
            #if _MSC_VER
                now = clock::now();
            #endif
            do {
                #if _MSC_VER // on Win32 wait_for is faster
                    duration remaining = until - now;
                    (void)Waiter.wait_for(lock, remaining); // noexcept
                #else // on GCC wait_until is faster
                    (void)Waiter.wait_until(lock, until); // may throw
                #endif
                if (Head != Tail) return true; // got an item
                if (Cleared) return false; // give up immediately
                now = clock::now();
            } while (now < until); // handle spurious wakeups
            return false;
        }
        void notify_one_unlocked() noexcept
        {
            Waiter.notify_one();
        }
        void notify_all_unlocked() noexcept
        {
            Waiter.notify_all();
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
        void pop_unlocked() noexcept
        {
            T* head = Head++;
            if constexpr (!std::is_trivially_destructible_v<T>)
                head->~T();

            if (Head == Tail) // clear the queue if Head catches the Tail
            {
                clear_unlocked();
            }
        }
        void ensure_capacity() noexcept
        {
            const size_t oldCap = capacity();
            // we have enough capacity, just shift the items to the front
            if (oldCap > 0 && size_t(Head - ItemsStart) >= (oldCap / 2))
            {
                // unshift elements to the front of the queue
                T* newStart = ItemsStart;
                T* newTail = move_items(Head, Tail, newStart);
                Head = newStart;
                Tail = newTail;
            }
            else // grow
            {
                size_t growBy = oldCap ? oldCap : 32;
                if (growBy > (16*1024)) growBy = 16*1024;
                const size_t newCap = oldCap + growBy;
                grow_to(newCap);
            }
            Cleared = false; // reset the cleared flag
        }
        void grow_to(size_t newCap) noexcept
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
                size_t count = (oldTail - oldHead);
                if (count)
                    ::memmove(newStart, oldHead, count * sizeof(T));
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

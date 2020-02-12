/**
 * Basic concurrent queue with several synchronization helpers, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#pragma once
#include <deque>
#include <mutex>
#include <condition_variable>

namespace rpp
{
    using std::mutex;
    using std::lock_guard;
    
    /**
     * @note This is not optimized for speed
     */
    template<class T>
    class concurrent_queue
    {
        std::deque<T> Queue;
        mutable mutex Mutex;
        std::condition_variable Waiter;
    public:
    
        ~concurrent_queue()
        {
            clear(); // safely lock, pop and notify all waiters to give up
        }
        
        bool empty() const { return Queue.empty(); }
        int  size()  const { return (int)Queue.size(); }
        int safe_size() const
        {
            lock_guard<mutex> lock {Mutex};
            return (int)Queue.size();
        }
        
        mutex& sync() const { return Mutex; }
        void notify() { Waiter.notify_all(); }
        
        template<class ChangeWaitFlags>
        void notify(const ChangeWaitFlags& changeFlags)
        {
            { lock_guard<mutex> lock {Mutex};
                changeFlags(); }
            Waiter.notify_all();
        }

        void clear()
        {
            { lock_guard<mutex> lock {Mutex};
                Queue.clear(); }
            Waiter.notify_all();
        }
        
        void push(T&& item)
        {
            { lock_guard<mutex> lock {Mutex};
                Queue.emplace_back(std::move(item)); }
            Waiter.notify_all();
        }

        void push(const T& item)
        {
            { lock_guard<mutex> lock {Mutex};
                Queue.push_back(item); }
            Waiter.notify_all();
        }

        // pushes an item without calling notify_all()
        void push_no_notify(T&& item)
        {
            { lock_guard<mutex> lock {Mutex};
                Queue.emplace_back(std::move(item)); }
        }

        T pop()
        {
            T item;
            { lock_guard<mutex> lock {Mutex};
                if (Queue.empty()) throw std::runtime_error("concurrent_queue<T>::pop(): Queue was empty!");
                pop_unlocked(item); }
            Waiter.notify_all();
            return item;
        }

        std::deque<T> atomic_copy() const
        {
            lock_guard<mutex> lock {Mutex};
            return Queue;
        }

        bool try_pop(T& outItem)
        {
            lock_guard<mutex> lock {Mutex};
            if (Queue.empty())
                return false;
            pop_unlocked(outItem);
            return true;
        }

        /**
         * Waits until an item is ready to be popped.
         * The cancellationCondition is used to terminate the wait:
         * @param outItem [out] The popped item. Only valid if return value is TRUE
         * @param timeoutMillis Maximum number of milliseconds to wait
         * @param cancellationCondition Cancellation condition, CANCEL if cancellationCondition()==true
         * @param cancellationInterval Interval in milliseconds for checking cancellation condition
        * @return TRUE if an item was popped,
         *        FALSE if no item popped due to: timeout or cancellation
         * @code
         *   string item;
         *   if (queue.wait_pop(item, [&]{ return Cancelled || Finished; })
         *   {
         *       // item is valid
         *   }
         * @endcode
         */
        template<class WaitUntil>
        bool wait_pop(T& outItem, const WaitUntil& cancellationCondition,
                      int cancellationInterval = 1000)
        {
            std::chrono::milliseconds interval {cancellationInterval};
            std::unique_lock<mutex> lock {Mutex};
            
            while (Queue.empty())
            {
                if (cancellationCondition())
                    break;
                Waiter.wait_for(lock, interval);
            }
            
            if (Queue.empty())
                return false;
            pop_unlocked(outItem);
            return true;
        }

        /**
         * Waits up to N milliseconds until an item is ready to be popped.
         * The cancellationCondition is used to terminate the wait:
         * @param outItem [out] The popped item. Only valid if return value is TRUE
         * @param timeoutMillis Maximum number of milliseconds to wait before returning FALSE
         * @param cancellationCondition Cancellation condition, CANCEL if cancellationCondition()==true
         * @return TRUE if an item was popped,
         *         FALSE if no item popped due to: timeout or cancellation
         * @code
         *   string item;
         *   if (queue.wait_pop(item, 1000, [&]{ return Cancelled || Finished; })
         *   {
         *       // item is valid
         *   }
         * @endcode
         */
        template<class WaitUntil>
        bool wait_pop(T& outItem, int timeoutMillis, const WaitUntil& cancellationCondition)
        {
            auto end = std::chrono::system_clock::now()
                     + std::chrono::milliseconds{timeoutMillis};
            std::unique_lock<mutex> lock {Mutex};
            
            while (Queue.empty())
            {
                if (cancellationCondition())
                    break;
                std::cv_status status = Waiter.wait_until(lock, end);
                if (status == std::cv_status::timeout)
                    break;
            }
            
            if (Queue.empty())
                return false;
            pop_unlocked(outItem);
            return true;
        }

        /**
         * Waits forever until an item is ready to be popped.
         * @warning If no items are present, then this will deadlock!
         */
        T wait_pop()
        {
            std::unique_lock<mutex> lock {Mutex};
            
            while (Queue.empty())
                Waiter.wait(lock);

            T result;
            pop_unlocked(result);
            return result;
        }
        
    private:
        inline void pop_unlocked(T& outItem)
        {
            outItem = std::move(Queue.front());
            Queue.pop_front();
        }
    };
}

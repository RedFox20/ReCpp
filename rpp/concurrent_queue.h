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
                Queue.emplace_back(std::forward<T>(item)); }
            Waiter.notify_all();
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
         * The cancellationCondition is used to terminate
         * the wait:
         * @code
         *   string item;
         *   if (queue.wait_pop(item, [&]{ return Cancelled || Finished; })
         *   {
         *       // item is valid
         *   }
         * @endcode
         */
        template<class WaitUntil>
        bool wait_pop(T& outItem, const WaitUntil& cancellationCondition)
        {
            std::unique_lock<mutex> lock {Mutex};
            
            while (Queue.empty() && !cancellationCondition())
                Waiter.wait(lock);
            
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
            outItem = move(Queue.front());
            Queue.pop_front();
        }
    };
}

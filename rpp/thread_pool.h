#pragma once
#ifndef RPP_THREAD_POOL_H
#define RPP_THREAD_POOL_H

#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "delegate.h"

namespace rpp
{
    using namespace std;

    //////////////////////////////////////////////////////////////////////////////////////////

    // Optimized Action delegate, using the 'Fastest Possible C++ Delegates' method
    // This supports lambdas by grabbing a pointer to the lambda instance,
    // so its main intent is to be used during blocking calls like parallel_for
    // It's completely useless for async callbacks that expect lambdas to be stored
    template<class... TArgs> class action
    {
        using Func = void(*)(void* callee, TArgs...);
        void* Callee;
        Func Function;

        template<class T, void (T::*TMethod)(TArgs...)>
        static void proxy(void* callee, TArgs... args)
        {
            T* p = static_cast<T*>(callee);
            return (p->*TMethod)(forward<TArgs>(args)...);
        }

        template<class T, void (T::*TMethod)(TArgs...)const>
        static void proxy(void* callee, TArgs... args)
        {
            const T* p = static_cast<T*>(callee);
            return (p->*TMethod)(forward<TArgs>(args)...);
        }

    public:
        action() : Callee(nullptr), Function(nullptr) {}
        action(void* callee, Func function) : Callee(callee), Function(function) {}

        template<class T, void (T::*TMethod)(TArgs...)>
        static action from_function(T* callee)
        {
            return { (void*)callee, &proxy<T, TMethod> };
        }

        template<class T, void (T::*TMethod)(TArgs...)const>
        static action from_function(const T* callee)
        {
            return { (void*)callee, &proxy<T, TMethod> };
        }

        inline void operator()(TArgs... args) const
        {
            (*Function)(Callee, args...);
        }

        explicit operator bool() const { return Callee != nullptr; }
    };

    //////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Simple semaphore for notifying and waiting on events
     */
    class semaphore
    {
        mutex m;
        condition_variable cv;
        int value = 0;

    public:
        enum wait_result {
            notified,
            timeout,
        };
    
        semaphore() = default;
        explicit semaphore(int initialCount)
        {
            reset(initialCount);
        }

        int count() const { return value; }

        void reset(int newCount = 0)
        {
            value = newCount;
            if (newCount > 0)
                cv.notify_one();
        }

        void notify()
        {
            { lock_guard<mutex> lock{ m };
                ++value;
            }
            cv.notify_one();
        }

        void wait()
        {
            unique_lock<mutex> lock{ m };
            while (value <= 0) // wait until value is actually set
                cv.wait(lock);
            --value; // consume the value
        }

        /**
         * Waits while @taskIsRunning is TRUE and sets it to TRUE again before returning
         * This works well for atomic barriers, for example:
         * @code
         *   sync.wait_barrier_while(IsRunning);  // waits while IsRunning == true and sets it to true on return
         *   processTask();
         * @endocde
         * @param taskIsRunning Reference to atomic flag to wait on
         */
        void wait_barrier_while(atomic_bool& taskIsRunning)
        {
            if (!taskIsRunning) {
                taskIsRunning = true;
                return;
            }
            unique_lock<mutex> lock{ m };
            while (taskIsRunning)
                cv.wait(lock);
            taskIsRunning = true;
        }
        
        /**
         * Waits while @atomicFlag is FALSE and sets it to FALSE again before returning
         * This works well for atomic barriers, for example:
         * @code
         *   sync.wait_barrier_until(HasFinished);  // waits while HasFinished == false and sets it to false on return
         *   processResults();
         * @endocde
         * @param hasFinished Reference to atomic flag to wait on
         */
        void wait_barrier_until(atomic_bool& hasFinished)
        {
            if (hasFinished) {
                hasFinished = false;
                return;
            }
            unique_lock<mutex> lock{ m };
            while (!hasFinished)
                cv.wait(lock);
            hasFinished = false;
        }
        
        /**
         * @param timeoutSeconds Maximum number of seconds to wait for this semaphore to be notified
         * @return signalled if wait was successful or timeout if timeoutSeconds had elapsed
         */
        wait_result wait(double timeoutSeconds)
        {
            unique_lock<mutex> lock{ m };
            while (value <= 0)
            {
                chrono::duration<double> dur(timeoutSeconds);
                if (cv.wait_for(lock, dur) == cv_status::timeout)
                    return timeout;
            }
            --value;
            return notified;
        }

        wait_result wait(int timeoutMillis)
        {
            unique_lock<mutex> lock{ m };
            while (value <= 0)
            {
                chrono::milliseconds dur(timeoutMillis);
                if (cv.wait_for(lock, dur) == cv_status::timeout)
                    return timeout;
            }
            --value;
            return notified;
        }

        bool try_wait()
        {
            unique_lock<mutex> lock{ m };
            if (value > 0) {
                --value;
                return true;
            }
            return false;
        }
    };

    //////////////////////////////////////////////////////////////////////////////////////////

    template<class Signature>
    using task_delegate = delegate<Signature>;

    /**
     * A simple thread-pool task. Can run owning generic tasks using standard function<> and
     * also range non-owning tasks which use the impossibly fast delegate callback system.
     */
    class pool_task
    {
        mutex m;
        condition_variable cv;
        thread th;
        task_delegate<void()> genericTask;
        action<int, int> rangeTask;
        int rangeStart  = 0;
        int rangeEnd    = 0;
        int maxIdleTime = 15;
        volatile bool taskRunning = false; // an active task is being executed
        volatile bool killed      = false; // this pool_task is being destroyed/has been destroyed

    public:
        enum wait_result {
            finished,
            timeout,
        };

        bool running()  const noexcept { return taskRunning; }

        pool_task();
        ~pool_task() noexcept;

        // Sets the maximum idle time before this pool task is abandoned to free up thread handles
        // @param maxIdleSeconds Maximum number of seconds to remain idle. If set to 0, the pool task is kept alive forever
        void max_idle_time(int maxIdleSeconds = 15);

        // assigns a new parallel for task to run
        // @warning This range task does not retain any resources, so you must ensure
        //          it survives until end of the loop
        // undefined behaviour if called when already running
        void run_range(int start, int end, const action<int, int>& newTask) noexcept;

        // assigns a new generic task to run
        // undefined behaviour if called when already running
        void run_generic(task_delegate<void()>&& genericTask) noexcept;

        // wait for task to finish
        wait_result wait(int timeoutMillis = 0/*0=no timeout*/) noexcept;

        // kill the task and wait for it to finish
        wait_result kill(int timeoutMillis = 0/*0=no timeout*/) noexcept;

    private:
        void notify_task();
        void run() noexcept;
        bool got_task() const noexcept;
        bool wait_for_task(unique_lock<mutex>& lock) noexcept;
    };


    /**
     * A generic thread pool that can be used to group and control pool lifetimes
     * By default a global thread_pool is also available
     *
     * By design, nesting parallel for loops is detected as a fatal error, because
     * creating nested threads will not bring any performance benefits
     *
     * Accidentally running nested parallel_for can end up in 8*8=64 threads on an 8-core CPU,
     * which is why it's considered an error.
     */
    class thread_pool
    {
        mutex tasksMutex;
        vector<unique_ptr<pool_task>> tasks;
        int taskMaxIdleTime = 15; // new task timeout in seconds
        atomic_bool rangeRunning { false }; // whether parallel range is running or not
        
    public:

        // the default global thread pool
        static thread_pool global;

        thread_pool();
        ~thread_pool() noexcept;

        // number of thread pool tasks that are currently running
        int active_tasks() noexcept;

        // number of thread pool tasks that are in idle state
        int idle_tasks() noexcept;

        // number of running + idle tasks in this threadpool
        int total_tasks() const noexcept;

        // if you're completely done with the thread pool, simply call this to clean up the threads
        // returns the number of tasks cleared
        int clear_idle_tasks() noexcept;

        // starts a single range task atomically
        pool_task* start_range_task(size_t& poolIndex, int rangeStart, int rangeEnd, 
                                    const action<int, int>& rangeTask) noexcept;

        // sets a new max idle time for spawned tasks
        // this does not notify already idle tasks
        // @param maxIdleSeconds Maximum idle seconds before tasks are abandoned and thread handle is released
        //                       Setting this to 0 keeps pool tasks alive forever
        void max_task_idle_time(int maxIdleSeconds = 15) noexcept;

        /**
         * Runs a new Parallel For range task. Only ONE parallel for can be running, any kind of
         * parallel nesting is forbidden. This prevents quadratic thread explosion.
         *
         * This function will block until all parallel tasks have finished running
         *
         * @param rangeStart Usually 0
         * @param rangeEnd Usually vec.size()
         * @param rangeTask Non-owning callback action.
         */
        void parallel_for(int rangeStart, int rangeEnd, const action<int, int>& rangeTask) noexcept;

        template<class Func> 
        void parallel_for(int rangeStart, int rangeEnd, const Func& func) noexcept
        {
            parallel_for(rangeStart, rangeEnd, 
                action<int, int>::from_function<Func, &Func::operator()>(&func));
        }

        // runs a generic parallel task
        pool_task* parallel_task(task_delegate<void()>&& genericTask) noexcept;

        // return the number of physical cores
        static int physical_cores();
    };

    /**
     * @brief Runs parallel_for on the default global thread pool
     *
     * Runs a new Parallel For range task. Only ONE parallel for can be running, any kind of
     * parallel nesting is forbidden. This prevents quadratic thread explosion.
     *
     * This function will block until all parallel tasks have finished running
     *
     * @param rangeStart Usually 0
     * @param rangeEnd Usually vec.size()
     * @param func Non-owning callback action.
     */
    template<class Func>
    inline void parallel_for(int rangeStart, int rangeEnd, const Func& func) noexcept
    {
        thread_pool::global.parallel_for(rangeStart, rangeEnd,
            action<int, int>::from_function<Func, &Func::operator()>(&func));
    }

    /**
     * Runs a generic parallel task with no arguments on the default global thread pool
     * @note Returns immediately
     * @code
     * rpp::parallel_task([s] {
     *     run_slow_work(s);
     * });
     * @endcode
     */
    inline pool_task* parallel_task(task_delegate<void()>&& genericTask) noexcept
    {
        return thread_pool::global.parallel_task(move(genericTask));
    }

    /**
     * Runs a generic lambda with arguments
     * @note Returns immediately
     * @code 
     * rpp::parallel_task([](string s) {
     *     auto r = run_slow_work(s, 42);
     *     send_results(r);
     * }, "somestring"s);
     * @endcode
     */
    template<class Func, class A>
    inline pool_task* parallel_task(Func&& func, A&& a)
    {
        return thread_pool::global.parallel_task([func=move(func),a=move(a)]() mutable {
            func(move(a));
        });
    }
    template<class Func, class A, class B>
    inline pool_task* parallel_task(Func&& func, A&& a, B&& b)
    {
        return thread_pool::global.parallel_task([func=move(func),a=move(a),b=move(b)]() mutable {
            func(move(a), move(b));
        });
    }
    template<class Func, class A, class B, class C>
    inline pool_task* parallel_task(Func&& func, A&& a, B&& b, C&& c)
    {
        return thread_pool::global.parallel_task([func=move(func),a=move(a),b=move(b),c=move(c)]() mutable {
            func(move(a), move(b), move(c));
        });
    }
    template<class Func, class A, class B, class C, class D>
    inline pool_task* parallel_task(Func&& func, A&& a, B&& b, C&& c, D&& d)
    {
        return thread_pool::global.parallel_task([func=move(func),a=move(a),b=move(b),c=move(c),d=move(d)]() mutable {
            func(move(a), move(b), move(c), move(d));
        });
    }

    //////////////////////////////////////////////////////////////////////////////////////////

    /**
     * @return TRUE if @flag == @expectedValue and atomically sets @flag to @newValue
     */
    inline bool atomic_test_and_set(atomic_bool& flag, bool expectedValue = true, bool newValue = false)
    {
        return flag.compare_exchange_weak(expectedValue, newValue);
    }

    //////////////////////////////////////////////////////////////////////////////////////////

} // namespace rpp

#endif // RPP_THREAD_POOL_H

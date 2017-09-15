#include "thread_pool.h"
#include <chrono>
#include <cassert>
#include <csignal>
#if __APPLE__ || __linux__
    #include <pthread.h>
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////

    pool_task::~pool_task() noexcept
    {
        { lock_guard<mutex> lock(mtx);
            killed = true;
        }
        cv.notify_one();
        th.detach();
        
        fprintf(stderr, "Deleting pool_task, genericTask[3]=%p\n", genericTask.data[3]);
    }

    void pool_task::run_range(int start, int end, const action<int, int>& newTask) noexcept
    {
        { lock_guard<mutex> lock(mtx);
            if (taskRunning) {
                fprintf(stderr, "rpp::pool_task already running! Undefined behaviour!\n");
            }
            genericTask  = {};
            rangeTask    = newTask;
            rangeStart   = start;
            rangeEnd     = end;
            taskRunning  = true;
        }
        cv.notify_one();
    }

    void pool_task::run_generic(delegate<void()>&& newTask) noexcept
    {
        { lock_guard<mutex> lock(mtx);
            if (taskRunning) {
                fprintf(stderr, "rpp::pool_task already running! Undefined behaviour!\n");
            }
            genericTask  = move(newTask);
            rangeTask    = {};
            rangeStart   = 0;
            rangeEnd     = 0;
            taskRunning  = true;
        }
        cv.notify_one();
    }

    void pool_task::wait(int timeoutMillis) noexcept
    {
        //printf("wait for (%d, %d)\n", loopStart, loopEnd);
        while (taskRunning) // duplicate check is intentional (to avoid lock)
        {
            unique_lock<mutex> lock(mtx);
            if (!taskRunning)
                return;
            if (timeoutMillis)
            {
                if (cv.wait_for(lock, chrono::milliseconds(timeoutMillis)) == cv_status::timeout)
                    return;
            }
            else
            {
                cv.wait(lock);
            }
        }
    }

    static void segfault(int) { throw runtime_error("SIGSEGV"); }

    void pool_task::run() noexcept
    {
        #if __APPLE__ || __linux__
            pthread_setname_np("rpp::pool_task");
        #endif
        
        signal(SIGSEGV, segfault); // set SIGSEGV handler so we can catch it

        while (!killed)
        {
            if (wait_for_task())
            {
                try
                {
                    decltype(rangeTask)   range;
                    decltype(genericTask) generic;
                    
                    // consume the tasks atomically
                    { lock_guard<mutex> lock{mtx};
                        range   = move(rangeTask);
                        generic = move(genericTask);
                        taskRunning = true;
                    }
                    
                    if (range)
                    {
                        range(rangeStart, rangeEnd);
                    }
                    else
                    {
                        generic();
                        generic.reset();
                    }
                }
                catch (const exception& e)
                {
                    fprintf(stderr, "pool_task::run unhandled exception: %s\n", e.what());
                }
                catch (...) // prevent failure that would terminate the thread
                {
                    fprintf(stderr, "pool_task::run unhandled exception!\n");
                }
                
                taskRunning = false;
                if (killed)
                    return;
            }
            cv.notify_one();
        }
    }

    bool pool_task::got_task() const noexcept
    {
        return (bool)rangeTask || (bool)genericTask;
    }

    bool pool_task::wait_for_task() noexcept
    {
        unique_lock<mutex> lock {mtx};
        for (;;)
        {
            if (killed)
                return false;
            if ((bool)rangeTask || (bool)genericTask)
                return true;
            
            cv.wait(lock);
        }
        return true;
    }


    ///////////////////////////////////////////////////////////////////////////////

    thread_pool thread_pool::global;
    static int core_count;

#if _WIN32
    #include <Windows.h>
    static int num_physical_cores()
    {
        DWORD bytes = 0;
        GetLogicalProcessorInformation(nullptr, &bytes);
        vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> coreInfo(bytes / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        GetLogicalProcessorInformation(coreInfo.data(), &bytes);

        int cores = 0;
        for (auto& info : coreInfo)
        {
            if (info.Relationship == RelationProcessorCore)
                ++cores;
        }
        return cores;
    }
#else
    static int num_physical_cores()
    {
        return (int)thread::hardware_concurrency();
    }
#endif

    int thread_pool::physical_cores()
    {
        if (!core_count) core_count = num_physical_cores();
        return core_count;
    }

    thread_pool::thread_pool()
    {
        if (!core_count) core_count = num_physical_cores();
    }

    thread_pool::~thread_pool() noexcept
    {
        // defined destructor to prevent agressive inlining
    }

    int thread_pool::active_tasks() noexcept
    {
        int active = 0;
        for (auto& task : tasks) 
            if (task->running()) ++active;
        return active;
    }

    int thread_pool::idle_tasks() noexcept
    {
        int idle = 0;
        for (auto& task : tasks)
            if (!task->running()) ++idle;
        return idle;
    }

    int thread_pool::total_tasks() noexcept
    {
        return (int)tasks.size();
    }

    int thread_pool::clear_idle_tasks() noexcept
    {
        int cleared = 0;
        for (int i = 0; i < (int)tasks.size(); ++i)
        {
            if (!tasks[i]->running())
            {
                swap(tasks[i], tasks.back()); // erase_swap_last pattern
                tasks.pop_back();
                ++cleared;
            }
        }
        return cleared;
    }

    pool_task* thread_pool::new_task() noexcept
    {
        tasks.emplace_back(make_unique<pool_task>());
        return tasks.back().get();
    }

    pool_task* thread_pool::next_task(size_t& poolIndex) noexcept
    {
        for (; poolIndex < tasks.size(); ++poolIndex)
        {
            pool_task* task = tasks[poolIndex].get();
            if (!task->running())
                return task;
        }
        return new_task();
    }

    pool_task* thread_pool::get_task() noexcept
    {
        size_t i = 0;
        return next_task(i);
    }

    void thread_pool::parallel_for(int rangeStart, int rangeEnd, 
                                   const action<int, int>& rangeTask) noexcept
    {
        assert(!rangeRunning && "Fatal error: nested parallel loops are forbidden!");
        assert(core_count != 0);

        const int range = rangeEnd - rangeStart;
        if (range <= 0)
            return;

        const int cores = range < core_count ? range : core_count;
        const int len = range / cores;

        // only one physical core or only one task to run. don't run in a thread
        if (cores == 1)
        {
            rangeTask(0, len);
            return;
        }

        pool_task** active = (pool_task**)alloca(sizeof(pool_task*) * cores);
        {
            lock_guard<mutex> lock(poolMutex);
            rangeRunning = true;

            size_t poolIndex = 0;
            for (int i = 0; i < cores; ++i)
            {
                const int start = i * len;
                const int end   = i == cores - 1 ? rangeEnd : start + len;

                pool_task* task = next_task(poolIndex);
                active[i] = task;
                task->run_range(start, end, rangeTask);
            }
        }

        for (int i = 0; i < cores; ++i)
            active[i]->wait();

        rangeRunning = false;
    }

    pool_task* thread_pool::parallel_task(delegate<void()>&& genericTask) noexcept
    {
        pool_task* task = get_task();
        task->run_generic(move(genericTask));
        return task;
    }
}

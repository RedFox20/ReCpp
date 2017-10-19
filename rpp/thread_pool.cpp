#include "thread_pool.h"
#include <chrono>
#include <cassert>
#include <csignal>
#if __APPLE__ || __linux__
    #include <pthread.h>
    #include <sys/prctl.h>
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////

    pool_task::pool_task()
    {
        th = thread{[this] { run(); }};
    }

    pool_task::~pool_task() noexcept
    {
        { unique_lock<mutex> lock{m};
            killed = true;
        }
        cv.notify_all();
        th.join();
    }

    void pool_task::max_idle_time(int maxIdleSeconds) { maxIdleTime = maxIdleSeconds; }

    void pool_task::run_range(int start, int end, const action<int, int>& newTask) noexcept
    {
        if (taskRunning)
            fprintf(stderr, "rpp::pool_task already running! This can cause a deadlock!\n");

        { lock_guard<mutex> lock{m};
            genericTask  = {};
            rangeTask    = newTask;
            rangeStart   = start;
            rangeEnd     = end;
            taskRunning  = true;
        }
        if (th.joinable()) cv.notify_one();
        else {

            th = thread{[this] { run(); }}; // restart thread if needed
        }
    }

    void pool_task::run_generic(function<void()>&& newTask) noexcept
    {
        if (taskRunning)
            fprintf(stderr, "rpp::pool_task already running! This can cause a deadlock!\n");

        { lock_guard<mutex> lock{m};
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
            unique_lock<mutex> lock{m};
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

    #if _WIN32
    #include <Windows.h>
    #pragma pack(push,8)
    struct THREADNAME_INFO
    {
        DWORD dwType; // Must be 0x1000.
        LPCSTR szName; // Pointer to name (in user addr space).
        DWORD dwThreadID; // Thread ID (-1=caller thread).
        DWORD dwFlags; // Reserved for future use, must be zero.
    };
    #pragma pack(pop)
    void SetThreadName(const char* threadName)
    {
        THREADNAME_INFO info;
        info.dwType     = 0x1000;
        info.szName     = threadName;
        info.dwThreadID = (DWORD)-1;
        info.dwFlags    = 0;

        #pragma warning(push)
        #pragma warning(disable: 6320 6322)
            const DWORD MS_VC_EXCEPTION = 0x406D1388;
            __try {
                RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
            } __except (1){}
        #pragma warning(pop)
    }
    #endif

    static void segfault(int) { throw runtime_error("SIGSEGV"); }

    void pool_task::run() noexcept
    {
        static int pool_task_id;
        char name[32];
        snprintf(name, sizeof(name), "rpp_task_%d", pool_task_id++);

        #if __APPLE__
            pthread_setname_np(name);
        #elif __linux__
            if (pthread_setname_np(pthread_self(), name)) {
                // failed. try using prctl instead:
                prctl(PR_SET_NAME, name, 0, 0, 0);
            }
        #elif _WIN32
            SetThreadName(name);
        #endif
        
        signal(SIGSEGV, segfault); // set SIGSEGV handler so we can catch it

        //printf("%s start\n", name);
        while (!killed)
        {
            //printf("%s wait for task\n", name);
            if (!wait_for_task())
                break;

            try
            {
                decltype(rangeTask)   range;
                decltype(genericTask) generic;
                
                // consume the tasks atomically
                { lock_guard<mutex> lock{m};
                    range   = move(rangeTask);
                    generic = move(genericTask);
                    rangeTask   = {};
                    genericTask = {}; // BUG: Clang on android doesn't move genericTask properly
                    taskRunning = true;
                }
                
                if (range)
                {
                    //printf("%s range [%d,%d) \n", name, rangeStart, rangeEnd);
                    range(rangeStart, rangeEnd);
                }
                else
                {
                    //printf("%s task\n", name);
                    generic();
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
            
            { lock_guard<mutex> lock{m};
                taskRunning = false;
                cv.notify_all();
            }
        }
        //printf("%s killed: %d\n", name, killed);
    }

    bool pool_task::got_task() const noexcept
    {
        return (bool)rangeTask || (bool)genericTask;
    }

    bool pool_task::wait_for_task() noexcept
    {
        unique_lock<mutex> lock {m};
        for (;;)
        {
            if (killed)
                return false;
            if (got_task())
                return true;
            if (maxIdleTime)
            {
                if (cv.wait_for(lock, chrono::seconds(maxIdleTime)) == cv_status::timeout)
                    return false;
            }
            else
            {
                cv.wait(lock);
            }
        }
    }


    ///////////////////////////////////////////////////////////////////////////////


    thread_pool thread_pool::global;
    static int core_count;

#if _WIN32
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
        lock_guard<recursive_mutex> lock{poolMutex};
        int active = 0;
        for (auto& task : tasks) 
            if (task->running()) ++active;
        return active;
    }

    int thread_pool::idle_tasks() noexcept
    {
        lock_guard<recursive_mutex> lock{poolMutex};
        int idle = 0;
        for (auto& task : tasks)
            if (!task->running()) ++idle;
        return idle;
    }

    int thread_pool::total_tasks() const noexcept
    {
        return (int)tasks.size();
    }

    int thread_pool::clear_idle_tasks() noexcept
    {
        lock_guard<recursive_mutex> lock{poolMutex};
        int cleared = 0;
        for (int i = 0; i < (int)tasks.size();)
        {
            if (!tasks[i]->running())
            {
                swap(tasks[i], tasks.back()); // erase_swap_last pattern
                tasks.pop_back();
                ++cleared;
            }
            else ++i;
        }
        return cleared;
    }

    pool_task* thread_pool::new_task() noexcept
    {
        lock_guard<recursive_mutex> lock{poolMutex};
        tasks.emplace_back(make_unique<pool_task>());
        return tasks.back().get();
    }

    pool_task* thread_pool::next_task(size_t& poolIndex) noexcept
    {
        lock_guard<recursive_mutex> lock{poolMutex};
        for (; poolIndex < tasks.size(); ++poolIndex)
        {
            pool_task* task = tasks[poolIndex].get();
            if (!task->running())
            {
                ++poolIndex;
                return task;
            }
        }
        return new_task();
    }

    pool_task* thread_pool::get_task() noexcept
    {
        size_t i = 0;
        return next_task(i);
    }

    void thread_pool::max_task_idle_time(int maxIdleSeconds) noexcept
    {
        taskMaxIdleTime = maxIdleSeconds;
        for (auto& task : tasks)
            task->max_idle_time(taskMaxIdleTime);
    }

    void thread_pool::parallel_for(int rangeStart, int rangeEnd, 
                                   const action<int, int>& rangeTask) noexcept
    {
        assert(!rangeRunning && "Fatal error: nested parallel loops are forbidden!");
        assert(core_count != 0 && "The appears to be no hardware concurrency");

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
        //vector<pool_task*> active(cores, nullptr);
        {
            lock_guard<recursive_mutex> lock{poolMutex};
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

    pool_task* thread_pool::parallel_task(function<void()>&& genericTask) noexcept
    {
        pool_task* task = get_task();
        task->run_generic(move(genericTask));
        return task;
    }
}

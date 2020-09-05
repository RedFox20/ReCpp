#include "thread_pool.h"
#include <chrono>
#include <cassert>
#include <csignal>
#include <cmath> // round
#include <unordered_map>
#if __APPLE__ || __linux__
# include <pthread.h>
#endif
#if __has_include("debugging.h")
# include "debugging.h"
#endif

#define POOL_TASK_DEBUG 0

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////

    static pool_trace_provider TraceProvider;
    
#if _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #  define WIN32_LEAN_AND_MEAN 1
    #endif
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
#endif

    void set_this_thread_name(const char* name)
    {
        #if __APPLE__
            pthread_setname_np(name);
        #elif __linux__
            pthread_setname_np(pthread_self(), name);
        #elif _WIN32
            THREADNAME_INFO info { 0x1000, name, DWORD(-1), 0 };
            #pragma warning(push)
            #pragma warning(disable: 6320 6322)
                const DWORD MS_VC_EXCEPTION = 0x406D1388;
                __try {
                    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
                } __except (1){}
            #pragma warning(pop)
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////////

#if POOL_TASK_DEBUG
#  ifdef LogWarning
#    define TaskDebug(fmt, ...) LogWarning(fmt, ##__VA_ARGS__)
#  else
#    define TaskDebug(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#  endif
#else
#  define TaskDebug(fmt, ...) // do nothing
#endif

#ifdef LogWarning
#  define UnhandledEx(fmt, ...) LogWarning(fmt, ##__VA_ARGS__)
#else
#  define UnhandledEx(fmt, ...) fprintf(stderr, "pool_task::unhandled_exception $ " fmt "\n", ##__VA_ARGS__)
#endif

    pool_task::pool_task()
    {
        // @note thread must start AFTER mutexes, flags, etc. are initialized, or we'll have a nasty race condition
        th = std::thread{[this] { run(); }};
    }

    pool_task::~pool_task() noexcept
    {
        kill();
    }

    void pool_task::max_idle_time(float maxIdleSeconds) noexcept { maxIdleTime = maxIdleSeconds; }

    bool pool_task::run_range(int start, int end, const action<int, int>& newTask) noexcept
    {
        std::lock_guard<std::mutex> lock{m};
        if (taskRunning) // handle this nasty race condition
            return false;

        trace.clear();
        error = nullptr;
        if (auto tracer = TraceProvider)
            trace = tracer();

        genericTask  = {};
        rangeTask    = newTask;
        rangeStart   = start;
        rangeEnd     = end;

        if (killed) {
            TaskDebug("resurrecting task");
            killed = false;
            (void)join_or_detach(finished);
            th = std::thread{[this] { run(); }}; // restart thread if needed
        }
        taskRunning = true;
        cv.notify_one();
        return true;
    }

    bool pool_task::run_generic(task_delegate<void()>&& newTask) noexcept
    {
        std::lock_guard<std::mutex> lock{m};
        if (taskRunning) // handle this nasty race condition
            return false;

        trace.clear();
        error = nullptr;
        if (auto tracer = TraceProvider)
            trace = tracer();

        genericTask  = std::move(newTask);
        rangeTask    = {};
        rangeStart   = 0;
        rangeEnd     = 0;

        if (killed) {
            TaskDebug("resurrecting task");
            killed = false;
            (void)join_or_detach(finished);
            th = std::thread{[this] { run(); }}; // restart thread if needed
        }
        taskRunning = true;
        cv.notify_one();
        return true;
    }

    pool_task::wait_result pool_task::wait(int timeoutMillis)
    {
        std::exception_ptr err;
        wait_result result = wait(timeoutMillis, std::nothrow, &err);
        if (err) std::rethrow_exception(err);
        return result;
    }

    pool_task::wait_result pool_task::wait(int timeoutMillis, std::nothrow_t,
                                           std::exception_ptr* outErr) noexcept
    {
        wait_result result = finished;
        std::unique_lock<std::mutex> lock{m};
        while (taskRunning && !killed)
        {
            if (timeoutMillis)
            {
                if (cv.wait_for(lock, milliseconds_t(timeoutMillis)) == std::cv_status::timeout)
                {
                    result = timeout;
                    break;
                }
                // else: loop back and see if we finished
            }
            else
            {
                cv.wait(lock);
                // loop back and see if we finished
            }
        }
        // NOTE: this is thread safe only thanks to the unique lock above
        if (auto e = error; outErr != nullptr && !*outErr)
        {
            *outErr = e;
        }
        return result;
    }

    pool_task::wait_result pool_task::kill(int timeoutMillis) noexcept
    {
        if (killed) {
            return join_or_detach(finished);
        }
        { std::unique_lock<std::mutex> lock{m};
            TaskDebug("killing task");
            killed = true;
        }
        cv.notify_all();
        wait_result result = wait(timeoutMillis, std::nothrow);
        return join_or_detach(result);
    }

    pool_task::wait_result pool_task::join_or_detach(wait_result result) noexcept
    {
        if (th.joinable())
        {
            if (result == timeout)
                th.detach();
            // can't join if we're on the same thread as the task itself
            // (can happen during exit())
            else if (std::this_thread::get_id() == th.get_id())
                th.detach();
            else
                th.join();
        }
        return result;
    }

    void pool_task::run() noexcept
    {
        static int pool_task_id;
        char name[32];
        snprintf(name, sizeof(name), "rpp_task_%d", pool_task_id++);
        set_this_thread_name(name);
        //TaskDebug("%s start", name);
        for (;;)
        {
            try
            {
                decltype(rangeTask)   range;
                decltype(genericTask) generic;
                
                // consume the Tasks atomically
                { std::unique_lock<std::mutex> lock{m};
                    //TaskDebug("%s wait for task", name);
                    if (!wait_for_task(lock)) {
                        TaskDebug("%s stop (%s)", name, killed ? "killed" : "timeout");
                        killed = true;
                        taskRunning = false;
                        cv.notify_all();
                        return;
                    }
                    range   = rangeTask;
                    generic = std::move(genericTask);
                    rangeTask   = {};
                    genericTask = {};
                    taskRunning = true;
                }
                if (range)
                {
                    //TaskDebug("%s(range_task[%d,%d))", name, rangeStart, rangeEnd);
                    range(rangeStart, rangeEnd);
                }
                else
                {
                    //TaskDebug("%s(generic_task)", name);
                    generic();
                }
            }
            // prevent failures that would terminate the thread
            catch (const std::exception& e) { unhandled_exception(e.what()); }
            catch (const char* e)           { unhandled_exception(e);        }
            catch (...)                     { unhandled_exception("");       }
            { std::lock_guard<std::mutex> lock{m};
                taskRunning = false;
                cv.notify_all();
            }
        }
    }

    void pool_task::unhandled_exception(const char* what) noexcept
    {
        //if (trace.empty()) UnhandledEx("%s", what);
        //else               UnhandledEx("%s\nTask Start Trace:\n%s", what, trace.c_str());
        error = std::current_exception();
    }

    bool pool_task::got_task() const noexcept
    {
        return (bool)rangeTask || (bool)genericTask;
    }

    bool pool_task::wait_for_task(std::unique_lock<std::mutex>& lock) noexcept
    {
        for (;;)
        {
            if (killed)
                return false;
            if (got_task())
                return true;
            if (maxIdleTime > 0.000001f)
            {
                if (cv.wait_for(lock, fseconds_t(maxIdleTime)) == std::cv_status::timeout)
                    return got_task(); // make sure to check for task even if it timeouts
            }
            else
            {
                cv.wait(lock);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////

    thread_pool& thread_pool::global()
    {
        static thread_pool globalPool;
        return globalPool;
    }

#if _WIN32
    static int num_physical_cores()
    {
        static int num_cores = []
        {
            DWORD bytes = 0;
            GetLogicalProcessorInformation(nullptr, &bytes);
            std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> coreInfo(bytes / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
            GetLogicalProcessorInformation(coreInfo.data(), &bytes);

            int cores = 0;
            for (auto& info : coreInfo)
            {
                if (info.Relationship == RelationProcessorCore)
                    ++cores;
            }
            return cores > 0 ? cores : 1;
        }();
        return num_cores;
    }
#else
    static int num_physical_cores()
    {
        static int num_cores = []
        {
            int n = (int)std::thread::hardware_concurrency() / 2;
            return n > 0 ? n : 1;
        }();
        return num_cores;
    }
#endif

    void thread_pool::set_task_tracer(pool_trace_provider traceProvider)
    {
        std::lock_guard<std::mutex> lock{ TasksMutex };
        TraceProvider = traceProvider;
    }

    thread_pool::thread_pool()
    {
        set_max_parallelism(num_physical_cores());
    }

    thread_pool::thread_pool(int max_parallelism)
    {
        set_max_parallelism(max_parallelism);
    }

    thread_pool::~thread_pool() noexcept
    {
        // defined destructor to prevent aggressive inlining and to manually control task destruction
        std::lock_guard<std::mutex> lock{TasksMutex};
        Tasks.clear();
    }
    
    void thread_pool::set_max_parallelism(int max_parallelism) noexcept
    {
        MaxParallelism = max_parallelism > 0 ? max_parallelism : 1;
    }
    
    void thread_pool::set_global_max_parallelism(int max_parallelism)
    {
        global().MaxParallelism = max_parallelism < 1 ? 1 : max_parallelism;
    }

    int thread_pool::global_max_parallelism()
    {
        return global().MaxParallelism;
    }

    int thread_pool::active_tasks() noexcept
    {
        std::lock_guard<std::mutex> lock{TasksMutex};
        int active = 0;
        for (auto& task : Tasks) 
            if (task->running()) ++active;
        return active;
    }

    int thread_pool::idle_tasks() noexcept
    {
        std::lock_guard<std::mutex> lock{TasksMutex};
        int idle = 0;
        for (auto& task : Tasks)
            if (!task->running()) ++idle;
        return idle;
    }

    int thread_pool::total_tasks() const noexcept
    {
        return static_cast<int>(Tasks.size());
    }

    int thread_pool::clear_idle_tasks() noexcept
    {
        std::lock_guard<std::mutex> lock{TasksMutex};
        int cleared = 0;
        for (size_t i = 0; i < Tasks.size();)
        {
            if (!Tasks[i]->running())
            {
                swap(Tasks[i], Tasks.back()); // erase_swap_last pattern
                Tasks.pop_back();
                ++cleared;
            }
            else ++i;
        }
        return cleared;
    }
    
    void thread_pool::max_task_idle_time(float maxIdleSeconds) noexcept
    {
        std::lock_guard<std::mutex> lock{TasksMutex};
        TaskMaxIdleTime = maxIdleSeconds;
        for (auto& task : Tasks)
            task->max_idle_time(TaskMaxIdleTime);
    }

    #define AssertTerminate(success, msg) \
        if (!(success)) { \
            LogError(msg); \
            std::terminate(); \
        }

    std::unique_ptr<pool_task> thread_pool::start_range_task(int rangeStart, int rangeEnd,
                                                             const action<int, int>& rangeTask) noexcept
    {
        {
            std::lock_guard<std::mutex> lock{TasksMutex};
            for (int i = (int)Tasks.size() - 1; i >= 0; --i)
            {
                auto& taskRef = Tasks[i];
                pool_task* pTask = taskRef.get();
                if (!pTask->running())
                {
                    if (pTask->run_range(rangeStart, rangeEnd, rangeTask))
                    {
                        pTask->max_idle_time(TaskMaxIdleTime);
                        auto t = std::move(taskRef);
                        Tasks.erase(Tasks.begin()+i);
                        return t;
                    }
                    // else: race condition (someone else held pointer to the task and restarted it)
                }
            }
        }

        // in this case we don't need any locking, because the task is not added to Pool
        auto t = std::make_unique<pool_task>();
        auto* task = t.get();
        task->max_idle_time(TaskMaxIdleTime);
        bool success = task->run_range(rangeStart, rangeEnd, rangeTask);
        AssertTerminate(success, "brand new pool_task->run_range() failed");
        return t;
    }

    void thread_pool::parallel_for(int rangeStart, int rangeEnd, int maxRangeSize,
                                   const action<int, int>& rangeTask)
    {
        if (rangeStart >= rangeEnd)
            return;

        const uint32_t range = rangeEnd - rangeStart;
        uint32_t maxTasks = MaxParallelism; // maximum number of Tasks to use
        uint32_t minTasks;  // minimum number of Tasks needed
        uint32_t maxLength; // max iter length in a single task except the last element,
                            // eg rng=11, cor=4 ==> len 3; resulting task lengths: 3,3,3,2
        if (maxRangeSize <= 0)
        {
            // not more Tasks than range size, range is guaranteed to be > 0
            minTasks = range < maxTasks ? range : maxTasks;
            // spread the range over all available cores, with last task getting the remainder
            // ex 11 / 4 --> round(2.75) --> 3
            // ex 128 / 18 --> round(7.11) --> 7
            maxLength = (int)roundf((float)range / minTasks);
        }
        else
        {
            // user specified minLength, ex: 1, 10, 10000
            maxLength = maxRangeSize;
            // see how many Tasks would be needed to satisfy task length
            // eg 50 / 1 => 50;  50 / 10 => 5;  50 / 10000 => 0.0025
            minTasks = (int)ceilf((float)range / maxLength);
            // clamp Tasks to number of physical cores
            // eg 50 => 4; 5 => 4; 1 => 1
            minTasks = maxTasks < minTasks ? maxTasks : minTasks;
        }

        // either the range is too small OR maxRangeSize calculated effective Tasks as 1
        // OR the number of physical cores is simply 1
        if (minTasks <= 1)
        {
            rangeTask(rangeStart, rangeEnd);
            return;
        }

        auto active = static_cast<std::unique_ptr<pool_task>*>(
                                alloca(sizeof(std::unique_ptr<pool_task>) * maxTasks));

        std::exception_ptr err;
        uint32_t spawned = 0; // actual number of spawned Tasks
        uint32_t nextWait = 0;

        for (int start = rangeStart; start < rangeEnd; start += maxLength)
        {
            int end = start + maxLength;
            if (end > rangeEnd) end = rangeEnd; // clamp the last task

            // we have enough tasks to choose from
            if (spawned < maxTasks)
            {
                // placement new on stack
                new (&active[spawned]) std::unique_ptr<pool_task>{
                    start_range_task(start, end, rangeTask)
                };
                ++spawned;
            }
            else
            {
                // we need to wait for an available task to solve the remainder
                // in most cases this will only be a single wait
                for (;; ++nextWait) // loop forever
                {
                    if (nextWait >= spawned) nextWait = 0;

                    auto& wait = active[nextWait];
                    wait->wait(0, std::nothrow, &err);
                    if (wait->run_range(start, end, rangeTask))
                        break; // great!

                    // boo! race condition, try again
                }
            }
        }

        if (spawned > maxTasks)
            ThrowErr("BUG: parallel_for spawned:%d > maxTasks:%d", spawned, maxTasks);

        // wait on all the spawned tasks to finish
        for (uint32_t i = 0; i < spawned; ++i)
        {
            active[i]->wait(0, std::nothrow, &err);
        }

        // and finally throw them back into the pool
        {
            std::lock_guard<std::mutex> lock{TasksMutex};
            for (uint32_t i = 0; i < spawned; ++i)
                Tasks.emplace_back(std::move(active[i]));
        }

        if (err) std::rethrow_exception(err);
    }

    pool_task* thread_pool::parallel_task(task_delegate<void()>&& genericTask) noexcept
    {
        { std::lock_guard<std::mutex> lock{TasksMutex};
            for (std::unique_ptr<pool_task>& t : Tasks)
            {
                pool_task* task = t.get();
                if (!task->running())
                {
                    if (task->run_generic(std::move(genericTask)))
                    {
                        return task;
                    }
                    // else: race condition (someone else held pointer to the task and restarted it)
                }
            }
        }
        
        // create and run a new task atomically
        auto t = std::make_unique<pool_task>();
        auto* task = t.get();
        task->max_idle_time(TaskMaxIdleTime);
        bool success = task->run_generic(std::move(genericTask));
        AssertTerminate(success, "brand new pool_task->run_generic() failed");

        std::lock_guard<std::mutex> lock{TasksMutex};
        Tasks.emplace_back(std::move(t));
        return task;
    }
}

#include "thread_pool.h"
#include <chrono>
#include <cassert>
#include <csignal>
#include <unordered_map>
#if __APPLE__ || __linux__
    #include <pthread.h>
#endif

#if __has_include("debugging.h")
#  include "debugging.h"
#endif

#define POOL_TASK_DEBUG 0

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////

    static pool_trace_provider TraceProvider;
    static pool_signal_handler SignalHandler = [](const char* what) {
        throw std::runtime_error(what);
    };

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
        th = thread{[this] { run(); }};
    }

    pool_task::~pool_task() noexcept
    {
        kill();
    }

    void pool_task::max_idle_time(int maxIdleSeconds) { maxIdleTime = maxIdleSeconds; }

    void pool_task::run_range(int start, int end, const action<int, int>& newTask) noexcept
    {
        assert(!taskRunning && "rpp::pool_task already running! This can cause deadlocks due to abandoned tasks!");

        { lock_guard<mutex> lock{m};
            trace.clear();
            error = nullptr;
            if (auto tracer = TraceProvider)
                trace = tracer();
            genericTask  = {};
            rangeTask    = newTask;
            rangeStart   = start;
            rangeEnd     = end;
            taskRunning  = true;
        }
        notify_task();
    }

    void pool_task::run_generic(task_delegate<void()>&& newTask) noexcept
    {
        assert(!taskRunning && "rpp::pool_task already running! This can cause deadlocks due to abandoned tasks!");

        //TaskDebug("queue task");
        { lock_guard<mutex> lock{m};
            trace.clear();
            error = nullptr;
            if (auto tracer = TraceProvider)
                trace = tracer();
            genericTask  = move(newTask);
            rangeTask    = {};
            rangeStart   = 0;
            rangeEnd     = 0;
            taskRunning  = true;
        }
        notify_task();
    }
    
    void pool_task::notify_task()
    {
        { lock_guard<mutex> lock{m};
            if (killed) {
                TaskDebug("resurrecting task");
                killed = false;
                if (th.joinable()) th.join();
                th = thread{[this] { run(); }}; // restart thread if needed
            }
        }
        cv.notify_one();
    }

    pool_task::wait_result pool_task::wait(int timeoutMillis)
    {
        unique_lock<mutex> lock{m};
        while (taskRunning)
        {
            if (timeoutMillis)
            {
                if (cv.wait_for(lock, chrono::milliseconds(timeoutMillis)) == cv_status::timeout)
                    return timeout;
            }
            else
            {
                cv.wait(lock);
            }
        }
        if (error) rethrow_exception(error);
        return finished;
    }

    pool_task::wait_result pool_task::kill(int timeoutMillis) noexcept
    {
        if (killed) {
            if (th.joinable()) th.join();
            return finished;
        }
        { unique_lock<mutex> lock{m};
            TaskDebug("killing task");
            killed = true;
        }
        cv.notify_all();
        wait_result result = wait(timeoutMillis);
        if (th.joinable()) {
            if (result == timeout) th.detach();
            else                   th.join();
        }
        return result;
    }

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

    // really nasty case where OS threads are just abandoned
    // with none of the destructors being run
    static mutex activeTaskMutex;
    static unordered_map<int, pool_task*> activeTasks;
    static int get_thread_id()
    {
        auto id = this_thread::get_id();
        return *(int*)&id;
    }
    static void register_thread_task(pool_task* task)
    {
        lock_guard<mutex> lock{ activeTaskMutex };
        activeTasks[get_thread_id()] = task;
    }
    static void erase_thread_task()
    {
        lock_guard<mutex> lock{ activeTaskMutex };
        activeTasks.erase(get_thread_id());
    }

    static void segfault(int) { SignalHandler("SIGSEGV"); }

    void pool_task::run() noexcept
    {
        static int pool_task_id;
        char name[32];
        snprintf(name, sizeof(name), "rpp_task_%d", pool_task_id++);

        #if __APPLE__
            pthread_setname_np(name);
        #elif __linux__
            pthread_setname_np(pthread_self(), name);
        #elif _WIN32
            SetThreadName(name);
        #endif
        
        signal(SIGSEGV, segfault); // set SIGSEGV handler so we can catch it
        register_thread_task(this);

        // mark all running threads killed during SIGTERM, before dtors run
        static int atExitRegistered;
        if (!atExitRegistered) atExitRegistered = !atexit([] {
            for (auto& task : activeTasks) {
                task.second->killed = true;
                task.second->cv.notify_all();
            }
            activeTasks.clear();
        });

        struct thread_exiter {
            pool_task* self;
            ~thread_exiter() {
                erase_thread_task();
                self->killed = true;
                self->taskRunning = false;
                self->cv.notify_all();
            }
        } markKilledOnScopeExit { this }; // however, this doesn't handle exit(0) where threads are mutilated

        //TaskDebug("%s start", name);
        for (;;)
        {
            try
            {
                decltype(rangeTask)   range;
                decltype(genericTask) generic;
                
                // consume the tasks atomically
                { unique_lock<mutex> lock{m};
                    //TaskDebug("%s wait for task", name);
                    if (!wait_for_task(lock)) {
                        TaskDebug("%s stop (%s)", name, killed ? "killed" : "timeout");
                        return;
                    }
                    range   = move(rangeTask);
                    generic = move(genericTask);
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
            catch (const exception& e) { unhandled_exception(e.what()); }
            catch (const char* e)      { unhandled_exception(e);        }
            catch (...)                { unhandled_exception("");       }
            { lock_guard<mutex> lock{m};
                taskRunning = false;
                cv.notify_all();
            }
        }
    }

    void pool_task::unhandled_exception(const char* what) noexcept
    {
        error = std::current_exception();

        string err = what;
        { lock_guard<mutex> lock{m};
            if (!trace.empty()) {
                err += "\nTask Start Trace:\n";
                err += trace;
            }
        }
        UnhandledEx("%s", err.c_str());
    }

    bool pool_task::got_task() const noexcept
    {
        return (bool)rangeTask || (bool)genericTask;
    }

    bool pool_task::wait_for_task(unique_lock<mutex>& lock) noexcept
    {
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

    thread_pool& thread_pool::global()
    {
        static thread_pool globalPool;
        return globalPool;
    }

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
        return global().coreCount;
    }

    void thread_pool::set_signal_handler(pool_signal_handler signalHandler)
    {
        lock_guard<mutex> lock{ tasksMutex };
        SignalHandler = signalHandler;
    }

    void thread_pool::set_task_tracer(pool_trace_provider traceProvider)
    {
        lock_guard<mutex> lock{ tasksMutex };
        TraceProvider = traceProvider;
    }

    thread_pool::thread_pool()
    {
        coreCount = num_physical_cores();
    }

    thread_pool::~thread_pool() noexcept
    {
        // defined destructor to prevent agressive inlining
    }

    int thread_pool::active_tasks() noexcept
    {
        lock_guard<mutex> lock{tasksMutex};
        int active = 0;
        for (auto& task : tasks) 
            if (task->running()) ++active;
        return active;
    }

    int thread_pool::idle_tasks() noexcept
    {
        lock_guard<mutex> lock{tasksMutex};
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
        lock_guard<mutex> lock{tasksMutex};
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

    pool_task* thread_pool::start_range_task(size_t& poolIndex, int rangeStart, int rangeEnd, 
                                             const action<int, int>& rangeTask) noexcept
    {
        { lock_guard<mutex> lock{tasksMutex};
            for (; poolIndex < tasks.size(); ++poolIndex)
            {
                pool_task* task = tasks[poolIndex].get();
                if (!task->running())
                {
                    ++poolIndex;
                    task->run_range(rangeStart, rangeEnd, rangeTask);
                    return task;
                }
            }
        }

        auto t  = make_unique<pool_task>();
        auto* task = t.get();
        task->max_idle_time(taskMaxIdleTime);
        task->run_range(rangeStart, rangeEnd, rangeTask);

        lock_guard<mutex> lock{tasksMutex};
        tasks.emplace_back(move(t));
        return task;
    }

    void thread_pool::max_task_idle_time(int maxIdleSeconds) noexcept
    {
        lock_guard<mutex> lock{tasksMutex};
        taskMaxIdleTime = maxIdleSeconds;
        for (auto& task : tasks)
            task->max_idle_time(taskMaxIdleTime);
    }

    void thread_pool::parallel_for(int rangeStart, int rangeEnd, 
                                   const action<int, int>& rangeTask) noexcept
    {
        assert(!rangeRunning && "Fatal error: nested parallel loops are forbidden!");
        assert(coreCount > 0 && "There appears to be no hardware concurrency");

        const int range = rangeEnd - rangeStart;
        if (range <= 0)
            return;

        const int cores = range < coreCount ? range : coreCount;
        const int len = range / cores;

        // only one physical core or only one task to run. don't run in a thread
        if (cores <= 1)
        {
            rangeTask(0, len);
            return;
        }

        pool_task** active = (pool_task**)alloca(sizeof(pool_task*) * cores);
        //vector<pool_task*> active(cores, nullptr);
        {
            rangeRunning = true;

            size_t poolIndex = 0;
            for (int i = 0; i < cores; ++i)
            {
                const int start = i * len;
                const int end   = i == cores - 1 ? rangeEnd : start + len;
                active[i] = start_range_task(poolIndex, start, end, rangeTask);
            }
        }

        for (int i = 0; i < cores; ++i)
            active[i]->wait();

        rangeRunning = false;
    }

    pool_task* thread_pool::parallel_task(task_delegate<void()>&& genericTask) noexcept
    {
        { lock_guard<mutex> lock{tasksMutex};
            for (unique_ptr<pool_task>& t : tasks)
            {
                pool_task* task = t.get();
                if (!task->running())
                {
                    task->run_generic(move(genericTask));
                    return task;
                }
            }
        }
        
        // create and run a new task atomically
        auto t = make_unique<pool_task>();
        auto* task = t.get();
        task->max_idle_time(taskMaxIdleTime);
        task->run_generic(move(genericTask));

        lock_guard<mutex> lock{tasksMutex};
        tasks.emplace_back(move(t));
        return task;
    }
}

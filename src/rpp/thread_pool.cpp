#include "thread_pool.h"
#include "timer.h"
#include "debugging.h"

#include <chrono>
#include <cassert>
#include <csignal>
#include <cmath> // round
#include <unordered_map>
#if __APPLE__ || __linux__
# include <pthread.h>
#endif

#define POOL_TASK_DEBUG 0
#define POOL_TASK_RPPLOG 0

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

    void set_this_thread_name(rpp::strview name) noexcept
    {
        #if _MSC_VER
            char threadName[33];
            THREADNAME_INFO info { 0x1000, name.to_cstr(threadName, sizeof(threadName)), DWORD(-1), 0 };
            #pragma warning(push)
            #pragma warning(disable: 6320 6322)
                const DWORD MS_VC_EXCEPTION = 0x406D1388;
                __try {
                    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
                } __except (1){}
            #pragma warning(pop)
        #else
            // pthread limit is 16 chars, including null terminator
            char threadName[16];
            size_t n = name.size() < 15 ? name.size() : 15;
            memcpy(threadName, name.data(), n);
            threadName[n] = '\0';
            #if __APPLE__
                pthread_setname_np(threadName);
            #elif __linux__
                pthread_setname_np(pthread_self(), threadName);
            #endif
        #endif
    }

    uint64 get_thread_id() noexcept
    {
        #if _WIN32
            return GetCurrentThreadId();
        #else
            return (uint64)pthread_self();
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////////

#if POOL_TASK_DEBUG
#  if POOL_TASK_RPPLOG
#    define TaskDebug(fmt, ...) LogWarning(fmt, ##__VA_ARGS__)
#  else
#    define TaskDebug(fmt, ...) do { \
        fprintf(stdout, "%s $ " fmt "\n", \
            rpp::TimePoint::now().time_of_day().to_string().c_str(), ##__VA_ARGS__); \
    } while(0)
#  endif
#  define InTaskDebug(...) __VA_ARGS__
#else
#  define TaskDebug(fmt, ...) // do nothing
#  define InTaskDebug(...)   // do nothing
#endif

#if POOL_TASK_RPPLOG
#  define UnhandledEx(fmt, ...) LogWarning(fmt, ##__VA_ARGS__)
#else
#  define UnhandledEx(fmt, ...) do { fprintf(stderr, "pool_task::unhandled_exception $ " fmt "\n", ##__VA_ARGS__); fflush(stderr); } while(0)
#endif

    ///////////////////////////////////////////////////////////////////////////////

    const char* pool_task_handle::worker_name() const noexcept
    {
        if (auto p = s)
            if (auto w = p->worker)
                return w->name;
        return "none";
    }

    wait_result pool_task_handle::wait(duration timeout) const
    {
        std::exception_ptr err;
        wait_result result = wait(timeout, std::nothrow, &err);
        if (err) std::rethrow_exception(err);
        return result;
    }

    wait_result pool_task_handle::wait(duration timeout, std::nothrow_t,
                                       std::exception_ptr* outErr) const noexcept
    {
        wait_result result = wait_result::finished;
        if (auto p = s)
        {
            auto lock = p->finished.spin_lock();
            if (p->finished.wait(lock, timeout) == rpp::semaphore::timeout)
                result = wait_result::timeout;
            if (auto e = p->error; outErr != nullptr && !*outErr)
                *outErr = e;
        }
        return result;
    }

    wait_result pool_task_handle::wait() const
    {
        std::exception_ptr err;
        wait_result result = wait(std::nothrow, &err);
        if (err) std::rethrow_exception(err);
        return result;
    }

    wait_result pool_task_handle::wait(std::nothrow_t, std::exception_ptr* outErr) const noexcept
    {
        wait_result result = wait_result::finished;
        if (auto p = s)
        {
            auto lock = p->finished.spin_lock();
            p->finished.wait(lock);
            if (auto e = p->error; outErr != nullptr && !*outErr)
                *outErr = e;
        }
        return result;
    }

    void pool_task_handle::signal_finished() noexcept
    {
        if (auto p = s)
        {
            auto lock = p->finished.spin_lock();
            p->finished.notify_all(lock);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////

    pool_worker::pool_worker()
    {
        // @note thread must start AFTER mutexes, flags, etc. are initialized, or we'll have a nasty race condition
        th = std::thread{[this] { run(); }};
    }

    pool_worker::pool_worker(float max_idle_seconds) : pool_worker{}
    {
        max_idle_timeout = std::chrono::milliseconds{int(max_idle_seconds*1000)};
    }

    pool_worker::~pool_worker() noexcept
    {
        kill();
    }

    void pool_worker::max_idle_time(float max_idle_seconds) noexcept
    {
        auto lock = new_task_flag.spin_lock();
        max_idle_timeout = std::chrono::milliseconds{int(max_idle_seconds*1000)};
    }

    pool_task_handle pool_worker::run_range(int start, int end, const action<int, int>& newTask) noexcept
    {
        auto lock = new_task_flag.spin_lock();
        // we always need to double-check if a task is already running
        if (current_task.is_running())
        {
            TaskDebug("%s task already running", name);
            return nullptr;
        }

        current_task = pool_task_handle{this};
        range_task   = newTask;
        range_start  = start;
        range_end    = end;
        new_task_flag.notify_all(lock);
        lock.unlock();

        if (killed)
        {
            TaskDebug("%s resurrecting", name);
            killed = false;
            (void)join_or_detach(wait_result::finished);
            th = std::thread{[this] { run(); }}; // restart thread if needed
        }

        // WARNING: this can be VERY slow
        if (auto tracer = TraceProvider)
            current_task.s->trace = tracer();

        return current_task;
    }

    pool_task_handle pool_worker::run_generic(task_delegate<void()>&& newTask) noexcept
    {
        auto lock = new_task_flag.spin_lock();
        // we always need to double-check if a task is already running
        if (current_task.is_running())
        {
            TaskDebug("%s task already running", name);
            return nullptr;
        }

        current_task = pool_task_handle{this};
        generic_task = std::move(newTask);
        new_task_flag.notify_all(lock);
        lock.unlock();

        if (killed)
        {
            TaskDebug("%s resurrecting", name);
            killed = false;
            (void)join_or_detach(wait_result::finished);
            th = std::thread{[this] { run(); }}; // restart thread if needed
        }

        // WARNING: this can be VERY slow
        if (auto tracer = TraceProvider)
            current_task.s->trace = tracer();

        return current_task;
    }

    wait_result pool_worker::kill(int timeoutMillis) noexcept
    {
        if (killed) {
            return join_or_detach(wait_result::finished);
        }

        new_task_flag.notify_all([&] {
            TaskDebug("%s set killed", name);
            killed = true;
        });

        // wait for runner to finish current task before joining the thread
        wait_result result = current_task.wait(std::chrono::milliseconds{timeoutMillis}, std::nothrow);
        return join_or_detach(result);
    }

    wait_result pool_worker::join_or_detach(wait_result result) noexcept
    {
        if (th.joinable())
        {
            if (result == wait_result::timeout)
            {
                TaskDebug("%s detaching", name);
                th.detach();
            }
            // can't join if we're on the same thread as the task itself
            // (can happen during exit())
            else if (std::this_thread::get_id() == th.get_id())
            {
                TaskDebug("%s detaching in same thread", name);
                th.detach();
            }
            else // finished
            {
                TaskDebug("%s joining", name);
                InTaskDebug(rpp::Timer t);
                th.join();
            #if POOL_TASK_DEBUG
                double elapsed_ms = t.elapsed_millis();
                TaskDebug("%s joining took: %.3fms", name, elapsed_ms);
                if (elapsed_ms >= 2.0) LogError("%s joining took excessively long", name);
            #endif
            }
        }
        return result;
    }

    void pool_worker::run() noexcept
    {
        static std::atomic_int pool_task_id;
        int this_task_id = ++pool_task_id;
        snprintf(name, sizeof(name), "rpp_task_%d", this_task_id);
        set_this_thread_name(name);
        TaskDebug("%s thread start", name);
        for (;;)
        {
            try
            {
                decltype(range_task)   range;
                decltype(generic_task) generic;

                // consume the Tasks atomically
                {
                    InTaskDebug(rpp::Timer t);
                    TaskDebug("%s wait_for_new_job", name);
                    auto lock = new_task_flag.spin_lock();
                    if (!wait_for_new_job(lock)) {
                        TaskDebug("%s wait_for_new_job stopped (%s)", name, killed ? "killed" : "timeout");
                        killed = true;
                        return;
                    }
                    TaskDebug("%s wait_for_new_job elapsed: %.3fms", name, t.elapsed_millis());
                    // copy the new task state
                    range   = range_task;
                    generic = std::move(generic_task);
                    range_task   = {};
                    generic_task = {};
                }
                InTaskDebug(rpp::Timer task_timer);
                if (range)
                {
                    TaskDebug("%s (RANGE[%d,%d))", name, range_start, range_end);
                    range(range_start, range_end);
                    TaskDebug("%s (RANGE[%d,%d)) elapsed %.3fms",
                              name, range_start, range_end, task_timer.elapsed_millis());
                }
                else if (generic)
                {
                    TaskDebug("%s (GENERIC)", name);
                    generic();
                    TaskDebug("%s (GENERIC) elapsed %.3fms",
                              name, task_timer.elapsed_millis());
                }
                else
                {
                    TaskDebug("%s (empty_task)", name);
                }

                // non-exceptional cleanup path:
                {
                    auto lock = new_task_flag.spin_lock();
                    current_task.signal_finished();
                }
            }
            // prevent failures that would terminate the thread
            catch (const std::exception& e) { unhandled_exception(e.what()); }
            catch (const char* e)           { unhandled_exception(e);        }
            catch (...)                     { unhandled_exception("");       }
        }
    }

    void pool_worker::unhandled_exception(const char* what) noexcept
    {
        //if (trace.empty()) UnhandledEx("%s", what);
        //else               UnhandledEx("%s\nTask Start Trace:\n%s", what, trace.c_str());
        (void)what;
        auto lock = new_task_flag.spin_lock();
        current_task.s->error = std::current_exception();
        current_task.signal_finished();
    }

    bool pool_worker::wait_for_new_job(std::unique_lock<mutex>& lock) noexcept
    {
        for (;;) // loop until new job, or killed
        {
            // wait for new task flag before checking anything else
            if (max_idle_timeout.count() > 0)
            {
                if (new_task_flag.wait(lock, max_idle_timeout) == rpp::semaphore::timeout)
                    return false; // timed out, we should kill the thread
            }
            else
            {
                // infinite wait for new task or kill signal
                new_task_flag.wait(lock);
            }

            // now that semaphore count is decremented, it's safe to check for killed
            // or current task
            if (killed)
                return false;
            if (current_task.is_running())
                return true;
        }
    }

    ///////////////////////////////////////////////////////////////////////////////

    thread_pool& thread_pool::global()
    {
        static thread_pool globalPool;
        return globalPool;
    }

#if _WIN32
    int num_physical_cores() noexcept
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
    int num_physical_cores() noexcept
    {
        static int num_cores = []
        {
            // TODO: figure out which types of CPU-s have SMT/HT
            #if MIPS || RASPI || OCLEA || __ANDROID__
                constexpr int hyperthreading_factor = 1;
            #else
                constexpr int hyperthreading_factor = 2;
            #endif
            int n = (int)std::thread::hardware_concurrency() / hyperthreading_factor;
            return n > 0 ? n : 1;
        }();
        return num_cores;
    }
#endif

    void thread_pool::set_task_tracer(pool_trace_provider traceProvider)
    {
        std::lock_guard lock{ TasksMutex };
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
        std::lock_guard lock{TasksMutex};
        Workers.clear();
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
        std::lock_guard lock{TasksMutex};
        int active = 0;
        for (auto& task : Workers) 
            if (task->running()) ++active;
        return active;
    }

    int thread_pool::idle_tasks() noexcept
    {
        std::lock_guard lock{TasksMutex};
        int idle = 0;
        for (auto& task : Workers)
            if (!task->running()) ++idle;
        return idle;
    }

    int thread_pool::total_tasks() const noexcept
    {
        return static_cast<int>(Workers.size());
    }

    int thread_pool::clear_idle_tasks() noexcept
    {
        std::lock_guard lock{TasksMutex};
        int cleared = 0;
        for (size_t i = 0; i < Workers.size();)
        {
            if (!Workers[i]->running())
            {
                swap(Workers[i], Workers.back()); // erase_swap_last pattern
                Workers.pop_back();
                ++cleared;
            }
            else ++i;
        }
        return cleared;
    }

    void thread_pool::max_task_idle_time(float maxIdleSeconds) noexcept
    {
        std::lock_guard lock{TasksMutex};
        TaskMaxIdleTime = maxIdleSeconds;
        for (auto& task : Workers)
            task->max_idle_time(TaskMaxIdleTime);
    }

    #define AssertTerminate(success, msg) \
        if (!(success)) { \
            LogError(msg); \
            std::terminate(); \
        }

    thread_pool::parallel_for_task
    thread_pool::start_range_task(int range_start, int range_end,
                                  const action<int, int>& range_task) noexcept
    {
        {
            std::lock_guard lock{TasksMutex};
            for (int i = (int)Workers.size() - 1; i >= 0; --i)
            {
                auto& worker_ref = Workers[i];
                pool_worker* worker = worker_ref.get();
                if (!worker->running())
                {
                    if (pool_task_handle task = worker->run_range(range_start, range_end, range_task);
                        task.was_started())
                    {
                        worker_ptr t = std::move(worker_ref);
                        Workers.erase(Workers.begin()+i);
                        return parallel_for_task{ std::move(t), std::move(task) };
                    }
                    // else: race condition (someone else held pointer to the task and restarted it)
                }
            }
        }

        // in this case we don't need any locking, because the task is not added to Pool
        auto w = std::make_unique<pool_worker>(TaskMaxIdleTime);
        pool_task_handle task = w->run_range(range_start, range_end, range_task);
        AssertTerminate(task.was_started(), "brand new pool_task->run_range() failed");
        return parallel_for_task{ std::move(w), std::move(task) };
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
            TaskDebug("parallel_for parallelism disabled");
            rangeTask(rangeStart, rangeEnd);
            return;
        }

        auto* active = static_cast<parallel_for_task*>(
            alloca(maxTasks * sizeof(parallel_for_task))
        );

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
                TaskDebug("parallel_for spawning %d < %d maxTasks", spawned, maxTasks);
                new (&active[spawned]) parallel_for_task{start_range_task(start, end, rangeTask)};
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
                    TaskDebug("parallel_for check_free %s", wait.task.worker_name());
                    // this 1ms wait is necessary, however it slightly slows down the loop
                    // during high contention, so we don't burn CPU trying to get a task
                    auto result = wait.task.wait(std::chrono::milliseconds{1}, std::nothrow, &err);
                    if (result == wait_result::finished)
                    {
                        // try to run next range on the same worker that just finished
                        InTaskDebug(rpp::Timer try_run);
                        TaskDebug("parallel_for check_free try_run %s", wait.task.worker_name());
                        if (auto task = wait.worker->run_range(start, end, rangeTask);
                            task.was_started())
                        {
                            wait.task = std::move(task);
                            TaskDebug("parallel_for wait_for_task try_run success %.3fms", try_run.elapsed_millis());
                            break; // great!
                        }
                        else
                        {
                            TaskDebug("parallel_for wait_for_task try_run failed");
                        }
                        // boo! race condition, try again
                    }
                }
            }
        }

        if (spawned > maxTasks)
            ThrowErr("BUG: parallel_for spawned:%d > maxTasks:%d", spawned, maxTasks);

        // wait on all the spawned tasks to finish
        TaskDebug("parallel_for waiting on %d tasks", spawned);
        InTaskDebug(rpp::Timer wait_on_all);
        // wait in reverse order, since last task is probably the slowest
        // and suspending this thread until then makes most sense
        for (uint32_t i = 0; i < spawned; ++i)
        // for (int32_t i = spawned - 1; i >= 0; --i)
        {
            active[i].task.wait(std::nothrow, &err);
        }
        TaskDebug("parallel_for wait_on_all %.3fms", wait_on_all.elapsed_millis());

        // and finally throw them back into the pool
        {
            std::lock_guard lock{TasksMutex};
            for (uint32_t i = 0; i < spawned; ++i)
            {
                Workers.emplace_back(std::move(active[i].worker));
                active[i].~parallel_for_task(); // manually clean up since we used alloca
            }
        }

        if (err) std::rethrow_exception(err);
    }

    pool_task_handle thread_pool::parallel_task(task_delegate<void()>&& generic_task) noexcept
    {
        { std::lock_guard lock{TasksMutex};
            for (worker_ptr& t : Workers)
            {
                pool_worker* worker = t.get();
                if (!worker->running())
                {
                    if (auto task = worker->run_generic(std::move(generic_task));
                        task.was_started())
                        return task;
                    // else: race condition (someone else held pointer to the task and restarted it)
                }
            }
        }
        
        // create and run a new task atomically
        auto w = std::make_unique<pool_worker>(TaskMaxIdleTime);
        auto task = w->run_generic(std::move(generic_task));
        AssertTerminate(task.was_started(), "brand new pool_task->run_generic() failed");

        std::lock_guard lock{TasksMutex};
        Workers.emplace_back(std::move(w));
        return task;
    }
}

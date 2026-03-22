#pragma once
/**
 * Fast cross-platform thread-pool, Copyright (c) 2017-2024, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
#  pragma warning(disable: 4251) // class 'std::*' needs to have dll-interface to be used by clients of struct 'rpp::*'
#endif
#include "config.h"
#include "threads.h"
#include "semaphore.h"
#include "delegate.h"
#include "strview.h"
#include "mutex.h"
#include <vector>
#if RPP_TSAN
#  include <sanitizer/tsan_interface.h>
#endif
#include <thread>
#include <string>
#include <atomic> // std::atomic_bool
#include <exception> // std::exception_ptr

// Feature flag: 1 = use rpp::atomic_shared_ptr for pool_task_handle
//               0 = use raw atomic pointer + manual refcount (old implementation)
#define RPP_POOL_TASK_USE_ATOMIC_SP 1

#if RPP_POOL_TASK_USE_ATOMIC_SP
#  include "atomic_shared_ptr.h"
#endif

struct test_threadpool; // forward declaration for unit tests

namespace rpp
{
    using seconds_t  = std::chrono::seconds;
    using fseconds_t = std::chrono::duration<float>;
    using dseconds_t = std::chrono::duration<double>;
    using milliseconds_t = std::chrono::milliseconds;
    template<class T> using duration_t = std::chrono::duration<T>;

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
            return (p->*TMethod)(std::forward<TArgs>(args)...);
        }

        template<class T, void (T::*TMethod)(TArgs...)const>
        static void proxy(void* callee, TArgs... args)
        {
            const T* p = static_cast<T*>(callee);
            return (p->*TMethod)(std::forward<TArgs>(args)...);
        }

    public:
        action() noexcept : Callee{nullptr}, Function{nullptr} {}
        action(void* callee, Func function) noexcept : Callee{callee}, Function{function} {}

        template<class T, void (T::*TMethod)(TArgs...)>
        static action from_function(T* callee) noexcept
        {
            return { (void*)callee, &proxy<T, TMethod> };
        }

        template<class T, void (T::*TMethod)(TArgs...)const>
        static action from_function(const T* callee) noexcept
        {
            return { (void*)callee, &proxy<T, TMethod> };
        }

        inline void operator()(TArgs... args) const
        {
            (*Function)(Callee, args...);
        }

        explicit operator bool() const noexcept { return Callee != nullptr; }
    };

    //////////////////////////////////////////////////////////////////////////////////////////

    template<class Signature> using task_delegate = rpp::delegate<Signature>;


    /**
     * Handles signals for pool Tasks. This is expected to throw an exception derived
     * from std::runtime_error
     */
    using pool_signal_handler = void (*)(const char* signal);


    /**
     * Provides a plain function which traces the current call stack
     */
    using pool_trace_provider = std::string (*)();


    //////////////////////////////////////////////////////////////////////////////////////////

    enum class wait_result : int
    {
        finished, // task finished successfully
        timeout,  // waiting on task timed out
    };

    class pool_worker;

    struct pool_task_state
    {
    #if !RPP_POOL_TASK_USE_ATOMIC_SP
        std::atomic_int ref_count;
    #endif
        rpp::semaphore_once_flag finished;
        std::string trace;
        std::exception_ptr error;
        pool_worker* worker = nullptr;
        explicit pool_task_state(pool_worker* w) noexcept
            :
        #if !RPP_POOL_TASK_USE_ATOMIC_SP
            ref_count{1},
        #endif
            worker{w} {}
    };

    /**
     * A waitable pool task handle that can be thread-safely passed around.
     *
     * Two implementations controlled by RPP_POOL_TASK_USE_ATOMIC_SP:
     *   1 = rpp::atomic_shared_ptr<pool_task_state> -- race-free, fixes load-then-increment UAF
     *   0 = raw atomic pointer + manual refcount -- original implementation
     */
    class RPPAPI pool_task_handle
    {
    #if RPP_POOL_TASK_USE_ATOMIC_SP
        rpp::atomic_shared_ptr<pool_task_state> ptr;
    #else
        std::atomic<pool_task_state*> ptr;
    #endif

    public:
        using duration = rpp::Duration;

        pool_task_handle(std::nullptr_t) noexcept : ptr{nullptr} {}
        explicit pool_task_handle(pool_worker* w) noexcept
            #if RPP_POOL_TASK_USE_ATOMIC_SP
                : ptr{std::make_shared<pool_task_state>(w)}
            #else
                : ptr{new pool_task_state{w}}
            #endif
        {
        }
        ~pool_task_handle() noexcept
        {
            #if RPP_POOL_TASK_USE_ATOMIC_SP
                ptr = nullptr; // atomic_shared_ptr will handle cleanup
            #else
                dec_ref(ptr.exchange(nullptr));
            #endif
        }
        pool_task_handle(const pool_task_handle& other) noexcept
            #if RPP_POOL_TASK_USE_ATOMIC_SP
                : ptr{other.ptr.load()}
            #endif
        {
            #if !RPP_POOL_TASK_USE_ATOMIC_SP
                ptr.store(inc_ref(other));
            #endif
        }
        pool_task_handle(pool_task_handle&& other) noexcept
            : ptr{other.ptr.exchange(nullptr)}
        {
        }
        pool_task_handle& operator=(const pool_task_handle& other) noexcept
        {
            if (this != &other) {
            #if RPP_POOL_TASK_USE_ATOMIC_SP
                ptr.store(other.ptr.load());
            #else
                dec_ref(ptr.exchange(inc_ref(other)));
            #endif
            }
            return *this;
        }
        pool_task_handle& operator=(pool_task_handle&& other) noexcept
        {
            if (this != &other) {
                ptr.store(other.ptr.exchange(nullptr));
            }
            return *this;
        }
    private:

    #if RPP_POOL_TASK_USE_ATOMIC_SP

        /// Returns a strong reference that keeps the state alive
        FINLINE std::shared_ptr<pool_task_state> get() const noexcept { return ptr.load(); }

        /// Atomically takes ownership, nulls this handle, returns owned ref
        FINLINE std::shared_ptr<pool_task_state> take() noexcept { return ptr.exchange(nullptr); }

    #else // !RPP_POOL_TASK_USE_ATOMIC_SP

        // tries to increment refcount, returns p if successful, nullptr if p is null or deleted
        // uses the finished mutex to serialize against dec_ref
        static pool_task_state* inc_ref(const pool_task_handle& handle) noexcept
        {
            auto* p = handle.ptr.load();
            if (!p) return nullptr;
            int old_refs = p->ref_count.fetch_add(1);
            if (old_refs <= 0 || old_refs > 100'000) {
                // instead of triggering an assert+terminate, log error and return nullptr gracefully
                if (old_refs == 0) LogWarning("invalid refcount=%d (detected use-after-free)", old_refs);
                else LogWarning("invalid refcount=%d (ref count overflow or memory corruption)", old_refs);
                return nullptr;
            }
            return p;
        }
        static void dec_ref(pool_task_state* p) noexcept
        {
            if (!p) return;
            int old_refs = p->ref_count.fetch_sub(1);
            if (old_refs != 1) return; // not the last reference
            #if RPP_TSAN
                __tsan_acquire(&p->ref_count);
            #endif
            delete p;
        }

        /// RAII strong ref proxy - to unify the interface with atomic_shared_ptr version
        /// Secondary ctor for taking full ownership of a pool task state
        struct strong_ref
        {
            pool_task_state* p;
            explicit strong_ref(const pool_task_handle& src) noexcept : p{inc_ref(src)} {}
            explicit strong_ref(pool_task_state* p) noexcept : p{p} {}
            ~strong_ref() noexcept { dec_ref(p); }
            strong_ref(const strong_ref&) = delete;
            strong_ref& operator=(const strong_ref&) = delete;
            explicit operator bool() const noexcept { return p != nullptr; }
            pool_task_state* operator->() const noexcept { return p; }
        };

        /// @returns a strong reference that keeps the state alive
        FINLINE strong_ref get() const noexcept { return strong_ref{*this}; }

        /// @returns strong ref by Atomically taking ownership, nulls this handle
        FINLINE strong_ref take() noexcept { return strong_ref{ptr.exchange(nullptr)}; }

    #endif // RPP_POOL_TASK_USE_ATOMIC_SP

    public:

        /// @returns The name of the worker thread that is running this task
        const char* worker_name() const noexcept;

        /// @brief True if task was started -- and it may have already finished @see is_finished
        bool was_started() const noexcept { return !!get(); }

        /// @returns True if task is running and has not finished yet
        bool is_running() const noexcept
        {
            auto strong = get();
            if (!strong) return false; // a null task cannot be running
            return !strong->finished.is_set();
        }

        /// @returns True if task has finished running
        bool is_finished() const noexcept
        {
            auto strong = get();
            if (!strong) return false; // a null task can never be finished
            return strong->finished.is_set();
        }

        // wait for task to finish with timeout
        // if timeout duration is 0, then task completion is checked atomically
        // @note Throws any unhandled exceptions from background thread
        //       This is similar to std::future behaviour
        // @param outErr [out] if outErr != null && *outErr != null, then *outErr
        //                     is initialized with the caught exception (if any)
        wait_result wait(rpp::Duration timeout) const;
        wait_result wait(rpp::Duration timeout, std::nothrow_t nothrow,
                         std::exception_ptr* outErr = nullptr) const noexcept;

        // wait for task to finish (no timeout)
        // @note Throws any unhandled exceptions from background thread
        //       This is similar to std::future behaviour
        // @param outErr [out] if outErr != null && *outErr != null, then *outErr
        //                     is initialized with the caught exception (if any)
        wait_result wait() const;
        wait_result wait(std::nothrow_t nothrow, std::exception_ptr* outErr = nullptr) const noexcept;

    private:
        friend class pool_worker;
        friend struct ::test_threadpool;
        void signal_finish_and_cleanup() noexcept;
    };

    //////////////////////////////////////////////////////////////////////////////////////////

    /**
     * A simple thread-pool task. Can run owning generic Tasks using standard function<> and
     * also range non-owning Tasks which use the impossibly fast delegate callback system.
     */
    class RPPAPI pool_worker
    {
        friend class pool_task_handle;
    private:
        rpp::semaphore_flag new_task_flag;
        std::thread th;
        char name[32] {};

        rpp::task_delegate<void()> generic_task;
        rpp::action<int, int> range_task;
        int range_start  = 0;
        int range_end    = 0;
        rpp::Duration max_idle_timeout = rpp::Duration::from_millis(15'000);

        pool_task_handle current_task { nullptr };
        std::atomic_bool killed = false; // this pool_worker is being killed, but might still resurrect
        std::atomic_bool destroying = false; // if true, the destructor is waiting for the thread to finish

    public:
        using duration = pool_task_handle::duration;
        using lock_t = typename rpp::semaphore::lock_t;

        pool_worker();
        pool_worker(float max_idle_seconds);
        ~pool_worker() noexcept;
        NOCOPY_NOMOVE(pool_worker)

        /**
         * @return TRUE if pool_worker is running a task
         */
        bool running() const noexcept
        {
            // must hold new_task_flag lock to prevent use-after-free:
            // pool_worker::run() deletes the pool_task_state under this same lock,
            // so without it, is_running() could dereference a freed pointer
            auto lock = new_task_flag.spin_lock();
            return current_task.is_running();
        }

        // Sets the maximum idle time before this pool task is abandoned to free up thread handles
        // @param max_idle_seconds Maximum number of seconds to remain idle. If set to 0, the pool task is kept alive forever
        void max_idle_time(float max_idle_seconds = 15) noexcept;

        // assigns a new parallel for task to run
        // @warning This range task does not retain any resources, so you must ensure
        //          it survives until end of the loop
        // undefined behaviour if called when already running
        // @param out If non-null, receives a copy of the task handle for waiting.
        // @return TRUE if run started successfully (task was not already running)
        [[nodiscard]] bool run_range(int start, int end, const action<int, int>& newTask, pool_task_handle* out) noexcept;

        // assigns a new generic task to run
        // undefined behaviour if called when already running
        // @param out If non-null, receives a copy of the task handle for waiting.
        //           If null (fire-and-forget), avoids refcount contention.
        // @return TRUE if run started successfully (task was not already running)
        [[nodiscard]] bool run_generic(task_delegate<void()>& newTask, pool_task_handle* out) noexcept;

        // kill the task and wait for it to finish
        wait_result kill(int timeoutMillis = 0/*0=no timeout*/) noexcept;

    private:
        void set_current_task_and_unlock(lock_t& lock, pool_task_handle* out) noexcept;
        void unhandled_exception(const char* what) noexcept;
        void run() noexcept;
        bool wait_for_new_job(lock_t& lock) noexcept;
        wait_result join_or_detach(wait_result result = wait_result::finished) noexcept;
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
    class RPPAPI thread_pool
    {
        using worker_ptr = std::unique_ptr<pool_worker>;

        struct parallel_for_task
        {
            worker_ptr worker;
            pool_task_handle task;
        };

        rpp::mutex TasksMutex;
        std::vector<worker_ptr> Workers;
        float TaskMaxIdleTime = 15; // new task timeout in seconds
        uint32_t MaxParallelism = 0; // maximum parallelism in parallel_for, 0 to disable

    public:
        using duration = pool_task_handle::duration;

        // the default global thread pool
        static thread_pool& global();

        thread_pool();

        /**
         * @param max_parallelism Sets the max number of concurrent tasks in parallel_for, 0 to disable parallelism
         * @see set_max_parallelism
         */
        explicit thread_pool(int max_parallelism);
        ~thread_pool() noexcept;
        NOCOPY_NOMOVE(thread_pool)
        
        /**
         * This sets the maximum number of concurrent tasks in parallel_for.
         * The default value is thread_pool::global_max_parallelism(),
         * however you can adjust this to fit your specific use cases.
         * Use 0 to disable parallelism.
         */
        void set_max_parallelism(int max_parallelism) noexcept;

        /** @return Max number of concurrent tasks of this thread pool */
        int max_parallelism() const { return MaxParallelism; }

        /**
         * Sets the max parallelism for the global thread pool.
         * Use 0 to disable parallelism.
         * @see set_max_parallelism
         */
        static void set_global_max_parallelism(int max_parallelism);

        /** @return Current Max number of concurrent tasks of the global thread pool */
        static int global_max_parallelism();

        // number of thread pool Tasks that are currently running
        int active_tasks() noexcept;

        // number of thread pool Tasks that are in idle state
        int idle_tasks() noexcept;

        // number of running + idle Tasks in this threadpool
        int total_tasks() const noexcept;

        // if you're completely done with the thread pool, simply call this to clean up the threads
        // returns the number of Tasks cleared
        int clear_idle_tasks() noexcept;

        /**
         * Sets a new max idle time for spawned Tasks
         * This does not notify already idle Tasks
         * @param max_idle_seconds Maximum idle seconds before Tasks are abandoned and thread handle is released
         *                       Setting this to 0 keeps pool Tasks alive forever
         */
        void max_task_idle_time(float max_idle_seconds = 15.0f) noexcept;

    private:
        // starts a single range task atomically
        // the task is removed from TaskPool to avoid concurrency issues with regular parallel tasks
        parallel_for_task start_range_task(int range_start, int range_end,
                                           const action<int, int>& range_task) noexcept;

        struct parallel_for_params
        {
            int max_tasks; // maximum number of Tasks to use, 0 disables parallelism
            int min_tasks;  // minimum number of Tasks needed
            int max_length; // max iter length in a single task except the last element,
                            // eg rng=11, cor=4 ==> len 3; resulting task lengths: 3,3,3,2

            // initializes parallel_for parameters based on range and max_range_size
            parallel_for_params(int range, int max_range_size, int max_parallelism) noexcept;
        };
    
    public:
    
        /**
         * Runs a new Parallel For range task.
         * This function will block until all parallel Tasks have finished running
         *
         * The callback function parameters [start, end) provide a range to iterate over,
         * which yields better loop performance. If your callback Tasks are heavy, then
         * consider `rpp::parallel_foreach`
         *
         * If range and max_range_size calculate # of Tasks as 1, then this will run sequentially
         *
         * @param range_start Usually 0
         * @param range_end Usually vec.size()
         * @param max_range_size Maximum range size for a single task to execute (if possible)
         *                     Ex: size=10 will execute Tasks as T0[0,10); T1[10,20); ...
         *                     For very slow individual Tasks, recommend max_range_size=1 so that T0[0,10); T1[1,2); T2[2,3); ...
         *                     Size 0 will attempt to auto-detect a reasonable size
         *                     For very fast Tasks, this should be high enough so that individual
         *                     task threads can maximize throughput.
         * @param range_task Non-owning callback action:  void(int start, int end)
         */
        void parallel_for(int range_start, int range_end, int max_range_size,
                          const action<int, int>& range_task);

        template<class Func> 
        void parallel_for(int range_start, int range_end, int max_range_size, const Func& func)
        {
            this->parallel_for(range_start, range_end, max_range_size,
                               action<int, int>::from_function<Func, &Func::operator()>(&func));
        }

        // runs a generic parallel task
        pool_task_handle parallel_task(task_delegate<void()>&& generic_task) noexcept;

        // runs a generic parallel task without returning a handle (fire-and-forget)
        // more efficient than parallel_task() when the caller doesn't need to wait on the handle
        void parallel_task_detached(task_delegate<void()>&& generic_task) noexcept;

    private:
        void start_generic_task(task_delegate<void()>&& generic_task, pool_task_handle* out) noexcept;

    public:

        /**
         * Enables tracing of parallel task calls. This makes it possible
         * to trace the callstack which triggered the parallel task, otherwise
         * there would be no hints where the it was launched if the task crashes.
         * @note This will slow down parallel task startup since the call stack is unwound for debugging
         * @warning This can severely impact performance, so only use for debugging purposes
         */
        void set_task_tracer(pool_trace_provider trace_provider);
    };


    /**
     * Runs parallel_for on the default global thread pool
     * This function will block until all parallel Tasks have finished running
     * 
     * The callback function parameters [start, end) provide a range to iterate over,
     * which yields better loop performance. If your callback Tasks are heavy, then
     * consider `rpp::parallel_foreach`
     *
     * If range and maxRangeSize calculate # of Tasks as 1, then this will run sequentially
     * 
     * @code
     * rpp::parallel_for(0, images.size(), 0, [&](int start, int end) {
     *     for (int i = start; i < end; ++i) {
     *         ProcessImage(images[i]);
     *     }
     * });
     * @endcode
     * @param rangeStart Usually 0
     * @param rangeEnd Usually vec.size()
     * @param maxRangeSize Maximum range size for a single task to execute (if possible)
     *                     Ex: size=10 will execute Tasks as T0[0,10); T1[10,20); ...
     *                     For very slow individual Tasks, recommend maxRangeSize=1 so that T0[0,10); T1[1,2); T2[2,3); ...
     *                     Size 0 will attempt to auto-detect a reasonable size
     *                     For very fast Tasks, this should be high enough so that individual
     *                     task threads can maximize throughput.
     * @param func Non-owning callback action:  void(int start, int end)
     */
    template<class Func>
    void parallel_for(int rangeStart, int rangeEnd, int maxRangeSize, const Func& func)
    {
        thread_pool::global().parallel_for(rangeStart, rangeEnd, maxRangeSize,
            action<int, int>::from_function<Func, &Func::operator()>(&func));
    }


    /**
     * Runs parallel_foreach on the default global thread pool
     * This function will block until all parallel Tasks have finished running
     * 
     * @code
     * std::vector<string> images = ... ;
     * rpp::parallel_foreach(images, [](string& image) {
     *     ProcessImage(image);
     * });
     * @endcode
     * @param items A random access container with an operator[](int index) and size()
     * @param foreach Non-owning foreach callback action:  void(auto item)
     */
    template<class Container, class ForeachFunc>
    void parallel_foreach(Container& items, const ForeachFunc& forEach)
    {
        thread_pool::global().parallel_for(0, (int)items.size(), 0, [&](int start, int end) {
            for (int i = start; i < end; ++i) {
                forEach(items[i]);
            }
        });
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
    inline pool_task_handle parallel_task(task_delegate<void()>&& genericTask) noexcept
    {
        return thread_pool::global().parallel_task(std::move(genericTask));
    }

    /**
     * Fire-and-forget version of parallel_task that doesn't return a handle.
     * More efficient: avoids refcount contention between caller and worker thread.
     */
    inline void parallel_task_detached(task_delegate<void()>&& genericTask) noexcept
    {
        thread_pool::global().parallel_task_detached(std::move(genericTask));
    }

#undef move_args
#define __get_nth_move_arg(_unused, _8, _7, _6, _5, _4, _3, _2, _1, N_0, ...) N_0
#define __move_args0(...)

#if _MSC_VER
#define __move_exp(x) x
#define __move_args1(x)      x=std::move(x)
#define __move_args2(x, ...) x=std::move(x),  __move_exp(__move_args1(__VA_ARGS__))
#define __move_args3(x, ...) x=std::move(x),  __move_exp(__move_args2(__VA_ARGS__))
#define __move_args4(x, ...) x=std::move(x),  __move_exp(__move_args3(__VA_ARGS__))
#define __move_args5(x, ...) x=std::move(x),  __move_exp(__move_args4(__VA_ARGS__))
#define __move_args6(x, ...) x=std::move(x),  __move_exp(__move_args5(__VA_ARGS__))
#define __move_args7(x, ...) x=std::move(x),  __move_exp(__move_args6(__VA_ARGS__))
#define __move_args8(x, ...) x=std::move(x),  __move_exp(__move_args7(__VA_ARGS__))

#define move_args(...) __move_exp(__get_nth_move_arg(0, ##__VA_ARGS__, \
        __move_args8, __move_args7, __move_args6, __move_args5, \
        __move_args4, __move_args3, __move_args2, __move_args1, __move_args0)(__VA_ARGS__))
#else
#define __move_args1(x)      x=std::move(x)
#define __move_args2(x, ...) x=std::move(x),  __move_args1(__VA_ARGS__)
#define __move_args3(x, ...) x=std::move(x),  __move_args2(__VA_ARGS__)
#define __move_args4(x, ...) x=std::move(x),  __move_args3(__VA_ARGS__)
#define __move_args5(x, ...) x=std::move(x),  __move_args4(__VA_ARGS__)
#define __move_args6(x, ...) x=std::move(x),  __move_args5(__VA_ARGS__)
#define __move_args7(x, ...) x=std::move(x),  __move_args6(__VA_ARGS__)
#define __move_args8(x, ...) x=std::move(x),  __move_args7(__VA_ARGS__)

#define move_args(...) __get_nth_move_arg(0, ##__VA_ARGS__, \
        __move_args8, __move_args7, __move_args6, __move_args5, \
        __move_args4, __move_args3, __move_args2, __move_args1, __move_args0)(__VA_ARGS__)
#endif


#undef forward_args
#define __get_nth_forward_arg(_unused, _8, _7, _6, _5, _4, _3, _2, _1, N_0, ...) N_0
#define __forward_args0(...)

#if _MSC_VER
#define __forward_exp(x) x
#define __forward_args1(x)      std::forward<decltype(x)>(x)
#define __forward_args2(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args1(__VA_ARGS__))
#define __forward_args3(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args2(__VA_ARGS__))
#define __forward_args4(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args3(__VA_ARGS__))
#define __forward_args5(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args4(__VA_ARGS__))
#define __forward_args6(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args5(__VA_ARGS__))
#define __forward_args7(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args6(__VA_ARGS__))
#define __forward_args8(x, ...) std::forward<decltype(x)>(x),  __forward_exp(__forward_args7(__VA_ARGS__))

#define forward_args(...) __forward_exp(__get_nth_forward_arg(0, ##__VA_ARGS__, \
        __forward_args8, __forward_args7, __forward_args6, __forward_args5, \
        __forward_args4, __forward_args3, __forward_args2, __forward_args1, __forward_args0)(__VA_ARGS__))
#else
#define __forward_args1(x)      x=std::move(x)
#define __forward_args2(x, ...) x=std::move(x),  __forward_args1(__VA_ARGS__)
#define __forward_args3(x, ...) x=std::move(x),  __forward_args2(__VA_ARGS__)
#define __forward_args4(x, ...) x=std::move(x),  __forward_args3(__VA_ARGS__)
#define __forward_args5(x, ...) x=std::move(x),  __forward_args4(__VA_ARGS__)
#define __forward_args6(x, ...) x=std::move(x),  __forward_args5(__VA_ARGS__)
#define __forward_args7(x, ...) x=std::move(x),  __forward_args6(__VA_ARGS__)
#define __forward_args8(x, ...) x=std::move(x),  __forward_args7(__VA_ARGS__)

#define forward_args(...) __get_nth_forward_arg(0, ##__VA_ARGS__, \
        __forward_args8, __forward_args7, __forward_args6, __forward_args5, \
        __forward_args4, __forward_args3, __forward_args2, __forward_args1, __forward_args0)(__VA_ARGS__)
#endif


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
    pool_task_handle parallel_task(Func&& func, A&& a)
    {
        return thread_pool::global().parallel_task([move_args(func, a)]() mutable {
            func(forward_args(a));
        });
    }
    template<class Func, class A, class B>
    pool_task_handle parallel_task(Func&& func, A&& a, B&& b)
    {
        return thread_pool::global().parallel_task([move_args(func, a, b)]() mutable {
            func(forward_args(a, b));
        });
    }
    template<class Func, class A, class B, class C>
    pool_task_handle parallel_task(Func&& func, A&& a, B&& b, C&& c)
    {
        return thread_pool::global().parallel_task([move_args(func, a, b, c)]() mutable {
            func(forward_args(a, b, c));
        });
    }
    template<class Func, class A, class B, class C, class D>
    pool_task_handle parallel_task(Func&& func, A&& a, B&& b, C&& c, D&& d)
    {
        return thread_pool::global().parallel_task([move_args(func, a, b, c, d)]() mutable {
            func(forward_args(a, b, c, d));
        });
    }

    //////////////////////////////////////////////////////////////////////////////////////////

} // namespace rpp
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace rpp
{
    using namespace std;

    // Optimized Action delegate, using the 'Fastest Possible C++ Delegates' method
    // This supports lambdas by grabbing a pointer to the lambda instance,
    // so its main intent is to be used during blocking calls like parallel_for
    // It's completely useless for async callbacks that expect lambdas to be stored
    template<class... TArgs> class action
    {
        using Func = void(*)(void* callee, TArgs...);
        void* Callee;
        Func Function;

        template <class T, void (T::*TMethod)(TArgs...)>
        static void methodCaller(void* callee, TArgs... args)
        {
            T* p = static_cast<T*>(callee);
            return (p->*TMethod)(args...);
        }

        template<class T, void (T::*TMethod)(TArgs...)const>
        static void methodCaller(void* callee, TArgs... args)
        {
            const T* p = static_cast<T*>(callee);
            return (p->*TMethod)(args...);
        }

    public:
        action() : Callee(nullptr), Function(nullptr) {}
        action(void* callee, Func function) : Callee(callee), Function(function) {}

        template<class T, void (T::*TMethod)(TArgs...)>
        static action from_function(T* callee)
        {
            return action((void*)callee, &methodCaller<T, TMethod>);
        }

        template<class T, void (T::*TMethod)(TArgs...)const>
        static action from_function(const T* callee)
        {
            return action((void*)callee, &methodCaller<T, TMethod>);
        }

        inline void operator()(TArgs... args) const
        {
            (*Function)(Callee, args...);
        }

        operator bool() const { return Callee != nullptr; }
    };


    class pool_task
    {
        mutex mtx;
        condition_variable cv;
        thread th = thread(&pool_task::run, this);
        action<int, int> task;
        int loopStart;
        int loopEnd;
        volatile bool taskRunning = false;
        volatile bool killed = false;

    public:

        bool running() const noexcept { return taskRunning; }

        ~pool_task() noexcept;

        // assigns a new task to run
        // undefined behaviour if called when running (parallel_for already manages this)
        void run_task(int start, int end, const action<int, int>& newTask) noexcept;

        // wait for task to finish
        void wait(int timeoutMillis = 0/*0=no timeout*/) noexcept;

    private:
        void run();
        bool wait_for_task();
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
        mutex poolmutex;
        vector<unique_ptr<pool_task>> tasks;
        bool loop_running;
        
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
        int total_tasks() noexcept;

        // if you're completely done with the thread pool, simply call this to clean up the threads
        // returns the number of tasks cleared
        int clear_idle_tasks() noexcept;

        // always creates a brand new task that is registered in the pool (and will get reused)
        pool_task* new_task() noexcept;

        pool_task* next_task(size_t& poolIndex) noexcept;

        void parallel_for(int rangeStart, int rangeEnd, const action<int, int>& rangeTask) noexcept;

        template<class Func> 
        void parallel_for(int rangeStart, int rangeEnd, const Func& func) noexcept
        {
            parallel_for(rangeStart, rangeEnd, 
                action<int, int>::from_function<Func, &Func::operator()>(&func));
        }
    };

    // runs parallel_for on the default global thread pool
    template<class Func> 
    inline void parallel_for(int rangeStart, int rangeEnd, const Func& func) noexcept
    {
        thread_pool::global.parallel_for(rangeStart, rangeEnd,
            action<int, int>::from_function<Func, &Func::operator()>(&func));
    }



} // namespace rpp

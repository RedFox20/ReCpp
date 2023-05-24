#pragma once
/**
 * Chainable and Coroutine compatible std::future extensions
 * Copyright (c) 2017-2018, 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <future>
#include "thread_pool.h"
#include <type_traits>

#if RPP_HAS_CXX20 && defined(__has_include) // Coroutines support for C++20
    #if __has_include(<coroutine>)
        #include <coroutine>
        #define RPP_HAS_COROUTINES 1
        #define RPP_CORO_STD std
    #elif __has_include(<experimental/coroutine>) // backwards compatibility for clang
        #include <experimental/coroutine>
        #define RPP_HAS_COROUTINES 1
        #define RPP_CORO_STD std::experimental
    #else
        #define RPP_HAS_COROUTINES 0
        #define RPP_CORO_STD
    #endif
#else
    #define RPP_HAS_COROUTINES 0
    #define RPP_CORO_STD
#endif

namespace rpp
{
    template<class T>
    class NODISCARD cfuture;

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Composable Promise that provides a way to set value from a task functor.
     * This allows chaining (composing) futures together.
     * 
     * Previous tasks are deallocated before running the next task in the chain,
     * to allow for very long task chains.
     * 
     * The result value is always moved from first task into the second one using std::move().
     */
    template<typename T>
    struct cpromise : std::promise<T>
    {
        /** set_value_from_task_result(task) */
        template<typename Task>
        void compose(Task& task) noexcept(noexcept(task()))
        {
            T value = task();
            // run task destructor before calling continuation
            // provides deterministic sequencing: DownloadAndSaveFile().then(OpenAndParseFile);
            if constexpr (!std::is_trivially_destructible_v<Task>) {
                Task t = std::move(task);
                (void)t;
            }
            // notify the awaiters that the value is set
            std::promise<T>::set_value(std::move(value));
        }
    };

    template<>
    struct cpromise<void> : std::promise<void>
    {
        /** set_value_from_task_result(task) */
        template<typename Task>
        void compose(Task& task) noexcept(noexcept(task()))
        {
            task();
            // run task destructor before calling continuation
            // provides deterministic sequencing: DownloadAndSaveFile().then(OpenAndParseFile);
            if constexpr (!std::is_trivially_destructible_v<Task>) {
                Task t = std::move(task);
                (void)t;
            }
            // notify the awaiters that the value is set
            std::promise<void>::set_value();
        }
    };


    /**
     * Runs any task delegate on the rpp::thread_pool using rpp::parallel_task()
     * @param task The task to run in background thread
     * @returns Composable future with return value set to task() return value.
     */
    template<typename Task>
    auto async_task(Task&& task) noexcept -> cfuture<decltype(task())>
    {
        using T = decltype(task());
        cpromise<T> p;
        std::shared_future<T> f = p.get_future().share();
        rpp::parallel_task([move_args(p, task)]() mutable noexcept
        {
            try {
                p.compose(task);
            } catch (...) {
                p.set_exception(std::current_exception());
            }
        });
        return f;
    }


    ////////////////////////////////////////////////////////////////////////////////


    namespace
    {
        template<typename T>
        struct function_traits : function_traits<decltype(&T::operator())> {
        };

        template<typename R, typename... Args>
        struct function_traits<R(Args...)> { // function type
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };
        
        template<typename R, typename... Args>
        struct function_traits<R (*)(Args...)> { // function pointer
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };

        template<typename R, typename... Args>
        struct function_traits<std::function<R(Args...)>> {
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };
        
        template<typename T, typename R, typename... Args>
        struct function_traits<R (T::*)(Args...)> { // member func ptr
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };

        template<typename T, typename R, typename... Args>
        struct function_traits<R (T::*)(Args...) const> {  // const member func ptr
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };

        template<typename T>
        using ret_type = typename function_traits<T>::ret_type;

        template<typename T>
        using first_arg_type = typename std::tuple_element<0, typename function_traits<T>::arg_types>::type;
    
        template<typename Future>
        using future_type = std::decay_t<decltype(std::declval<Future>().get())>;
    }


    ////////////////////////////////////////////////////////////////////////////////


    /**
     * Composable Futures with Coroutine support
     * 
     * Provides composable facilities for continuations:
     *   - then(): allows chaining futures together by passing the result to the next future
     *             and resuming it using rpp::async_task()
     *   - continue_with(): allows to continue execution without returning a future,
     *                      which is useful for the final step in chaining.
     *                      NOTE: *this will be moved into the rpp::async_task(),
     *                            which is needed to avoid having to block in the destructor
     *   - detach(): allows to abandon this future by moving the state into rpp::async_task()
     *               and waiting for finish there
     * 
     * Example with classic composable futures using callbacks
     * @code
     *     rpp::async_task([=]{
     *         return downloadZipFile(url);
     *     }).then([=](std::string zipPath) {
     *         return extractContents(zipPath);
     *     }).continue_with([=](std::string extractedDir) {
     *         callOnUIThread([=]{ jobComplete(extractedDir); });
     *     });
     * @endcode
     * 
     * Example with C++20 coroutines. This uses co_await <lambda> to run each lambda
     * in a background thread. This is useful if you are upgrading legacy Synchronous
     * code which doesn't have awaitable IO.
     * 
     * The lambdas are run in background thread using rpp::thread_pool
     * @code
     *     using namespace rpp::coro_operators;
     *     rpp::cfuture<void> downloadAndUnzipDataAsync(std::string url) {
     *         std::string zipPath = co_await [&]{ return downloadZipFile(url); };
     *         std::string extractedDir = co_await [&]{ return extractContents(zipPath); };
     *         co_await callOnUIThread([=]{ jobComplete(extractedDir); });
     *         co_return;
     *     }
     * @endcode
     */
    template<typename T>
    class NODISCARD cfuture : public std::shared_future<T>
    {
        using super = std::shared_future<T>;
    public:
        cfuture() noexcept = default;
        cfuture(std::future<T>&& f)        noexcept : super{std::move(f)} {}
        cfuture(std::shared_future<T>&& f) noexcept : super{std::move(f)} {}
        cfuture(cfuture&& f)      noexcept : super{std::move(f)} {}
        cfuture(const cfuture& f) noexcept : super{f} {}
        cfuture& operator=(std::future<T>&& f)        noexcept { super::operator=(std::move(f)); return *this; }
        cfuture& operator=(std::shared_future<T>&& f) noexcept { super::operator=(std::move(f)); return *this; }
        cfuture& operator=(cfuture&& f)          noexcept { super::operator=(std::move(f)); return *this; }
        cfuture& operator=(const cfuture& f)     noexcept { super::operator=(f); return *this; }
        ~cfuture() noexcept // always block if future is still incomplete
        {
            if (this->valid())
                this->wait();
        }

        /**
         * @brief Downcasts cfuture<T> into cfuture<void>, which allows to simply
         *        wait for a chain of futures to complete without getting a return value.
         * @code
         *     using namespace rpp::coro_operators;
         *     co_await async_task(operation1).then(operation2).then();
         * @endcode
         */
        cfuture<void> then() noexcept;

        /**
         * Continuation: after this future is complete, forwards the result to `task`.
         * The continuation is executed on a worker thread via rpp::async_task()
         * 
         * @code 
         *     auto f = async_task([=] {
         *         return downloadZip(url, onProgress);
         *     }).then([=](std::string tempFile) {
         *         return unzipContents(tempFile, onProgress);
         *     }).then([=](std::string filesDir) {
         *         loadComponents(filesDir, onProgress);
         *     });
         * @endcode
         */
        template<typename Task>
        cfuture<ret_type<Task>> then(Task&& task) noexcept
        {
            return rpp::async_task([f=std::move(*this), move_args(task)]() mutable {
                return task(f.get());
            });
        }

        /**
         * Continuation with exception handlers: allows to forward the task result
         * to the next task in chain, or handle an exception of the given type.
         * 
         * This allows for more complicated task chains where exceptions can be used
         * to recover the task chain and return a usable value.
         * 
         * @code
         *     async_task([=] {
         *         return loadCachedScene(getCachePath(file));
         *     }, [=](invalid_cache_state& e) {
         *         return loadCachedScene(downloadAndCacheFile(file)); // recover
         *     }).then([=](std::shared_ptr<SceneData> sceneData) {
         *         setCurrentScene(sceneData);
         *     }, [=](scene_load_failed& e) {
         *         loadDefaultScene(); // recover
         *     });
         * @endcode
         */
        template<typename Task, class ExceptHA>
        cfuture<ret_type<Task>>
        then(Task&& task, ExceptHA&& exhA) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            return async_task([f=std::move(*this), move_args(task, exhA)]() mutable {
                try { return task(f.get()); } 
                catch (ExceptA& a) { return exhA(a); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB>
        cfuture<ret_type<Task>>
        then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB)]() mutable {
                try { return task(f.get()); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB, class ExceptHC>
        cfuture<ret_type<Task>>
        then(Task task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
                try { return task(f.get()); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
                catch (ExceptC& c) { return exhC(c); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB, class ExceptHC, class ExceptHD>
        cfuture<ret_type<Task>>
        then(Task task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC, ExceptHD&& exhD) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            using ExceptD = first_arg_type<ExceptHD>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
                try { return task(f.get()); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
                catch (ExceptC& c) { return exhC(c); }
                catch (ExceptD& d) { return exhD(d); }
            });
        }


        /**
         * Similar to .then() continuation, but doesn't return an std::future
         * and detaches this future.
         * 
         * @warning This future will be empty after calling `continue_with()`.
         *          `*this` will be moved into the background thread because of being detached.
         */
        template<typename Task>
        void continue_with(Task&& task) noexcept
        {
            rpp::parallel_task([f=std::move(*this), move_args(task)]() mutable {
                (void)task(f.get());
            });
        }

        template<typename Task, class ExceptHA>
        void continue_with(Task&& task, ExceptHA&& exhA) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA)]() mutable {
                try { (void)task(f.get()); }
                catch (ExceptA& a) { (void)exhA(a); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB)]() mutable {
                try { (void)task(f.get()); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB, class ExceptHC>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
                try { (void)task(f.get()); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
                catch (ExceptC& c) { (void)exhC(c); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB, class ExceptHC, class ExceptHD>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC, ExceptHD&& exhD) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            using ExceptD = first_arg_type<ExceptHD>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
                try { (void)task(f.get()); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
                catch (ExceptC& c) { (void)exhC(c); }
                catch (ExceptD& d) { (void)exhD(d); }
            });
        }

        /**
         * Abandons this future and prevents any waiting in destructor
         * @warning This will swallow any exceptions
         */
        void detach() noexcept
        {
            if (this->valid())
            {
                rpp::parallel_task([f=std::move(*this)]() mutable {
                    try { (void)f.get(); }
                    catch (...) {}
                });
            }
        }

        /**
         * @brief Calls get() and then forcefully moves out the shared_future result value
         * This is useful for handling temporary cfuture<T> objects, or `mycoro().get_value()` calls.
         */
        [[nodiscard]] T get_value() const
        {
            return std::move((T&&)super::get()); // force-steal the internal shared_future result
        }

        /**
         * Gets the shared reference to the result.
         * @see get_value() if you want to explicitly move the value out of the future
         */
        [[nodiscard]] const T& get() const &
        {
            return super::get();
        }

        /**
         * When called on a temporary cfuture<T>, forcefully steals the internal result
         */
        [[nodiscard]] T get() const &&
        {
            return std::move((T&&)super::get()); // force-steal the internal shared_future result
        }

    #if RPP_HAS_COROUTINES // C++20 coro support
        // checks if the future is already finished
        bool await_ready() const noexcept
        {
            return this->wait_for(std::chrono::microseconds{0}) != std::future_status::timeout;
        }
        // suspension point that launches the background async task
        void await_suspend(RPP_CORO_STD::coroutine_handle<> cont) noexcept
        {
            rpp::parallel_task([this, cont]
            {
                if (this->valid())
                    this->wait();
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        // gets the result and will rethrow any exceptions from the cfuture
        const T& await_resume() const &
        {
            return super::get();
        }
        // when called on temporary cfuture objects, the result has to be forcefully moved out
        [[nodiscard]] T await_resume() const &&
        {
            return std::move((T&&)super::get()); // force-steal the internal shared_future result
        }
    #endif
    }; // cfuture<T>

    /**
     * Composable Futures with Coroutine support. See docs in cfuture<T>.
     */
    template<>
    class NODISCARD cfuture<void> : public std::shared_future<void>
    {
        using super = shared_future<void>;
    public:
        cfuture() noexcept = default;
        cfuture(std::future<void>&& f)   noexcept : super{std::move(f)} {}
        cfuture(shared_future<void>&& f) noexcept : super{std::move(f)} {}
        cfuture(cfuture&& f)      noexcept : super{std::move(f)} {}
        cfuture(const cfuture& f) noexcept : super{f} {} // NOLINT
        cfuture& operator=(std::future<void>&& f)   noexcept { super::operator=(std::move(f)); return *this; }
        cfuture& operator=(shared_future<void>&& f) noexcept { super::operator=(std::move(f)); return *this; }
        cfuture& operator=(cfuture&& f)      noexcept { super::operator=(std::move(f)); return *this; }
        cfuture& operator=(const cfuture& f) noexcept { super::operator=(f); return *this; }
        ~cfuture() noexcept // always block if future is still incomplete
        {
            if (this->valid())
                this->wait();
        }

        /**
         * @brief This is a no-op on cfuture<void>
         */
        cfuture<void> then() noexcept { return { std::move(*this) }; }

        template<typename Task>
        cfuture<ret_type<Task>> then(Task&& task) noexcept
        {
            return rpp::async_task([f=std::move(*this), move_args(task)]() mutable {
                f.get();
                return task();
            });
        }

        /**
         * Continuation with exception handlers: allows cfuture<void> to be followed
         * by another cfuture<T> or to recover on specific exceptions.
         * 
         * This allows for more complicated task chains where exceptions can be used
         * to recover the task chain.
         * 
         * @code
         *     // start with a void future
         *     async_task([=] {
         *         loadScene(persistentStorage().getLastScene());
         *     }, [=](scene_load_failed& e) {
         *         loadDefaultScene(); // recover
         *     }).then([=]() -> SceneComponents { // it can be continued by cfuture<T>
         *         return loadPluginComponents(plugin);
         *     }, [=](component_load_failed& e) {
         *         return {}; // ignore bad plugins
         *     });
         * @endcode
         */
        template<typename Task, class ExceptHA>
        cfuture<ret_type<Task>> then(Task&& task, ExceptHA&& exhA) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            return async_task([f=std::move(*this), move_args(task, exhA)]() mutable {
                try { f.get(); return task(); }
                catch (ExceptA& a) { return exhA(a); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB>
        cfuture<ret_type<Task>>
        then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB)]() mutable {
                try { f.get(); return task(); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB, class ExceptHC>
        cfuture<ret_type<Task>>
        then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
                try { f.get(); return task(); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
                catch (ExceptC& c) { return exhC(c); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB, class ExceptHC, class ExceptHD>
        cfuture<ret_type<Task>>
        then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC, ExceptHD&& exhD) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            using ExceptD = first_arg_type<ExceptHD>;
            return async_task([f=std::move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
                try { f.get(); return task(); }
                catch (ExceptA& a) { return exhA(a); }
                catch (ExceptB& b) { return exhB(b); }
                catch (ExceptC& c) { return exhC(c); }
                catch (ExceptD& d) { return exhD(d); }
            });
        }

        /**
         * Similar to .then() continuation, but doesn't return an std::future
         * and detaches this future.
         * 
         * @warning This future will be empty after calling `continue_with()`.
         *          `*this` will be moved into the background thread because of being detached.
         */
        template<typename Task>
        void continue_with(Task&& task) noexcept
        {
            rpp::parallel_task([f=std::move(*this), move_args(task)]() mutable {
                f.get();
                (void)task();
            });
        }

        template<typename Task, class ExceptHA>
        void continue_with(Task&& task, ExceptHA&& exhA) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA)]() mutable noexcept {
                try { f.get(); (void)task(); }
                catch (ExceptA& a) { (void)exhA(a); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB)]() mutable {
                try { f.get(); (void)task(); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB, class ExceptHC>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB, exhC)]() mutable {
                try { f.get(); (void)task(); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
                catch (ExceptC& c) { (void)exhC(c); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB, class ExceptHC, class ExceptHD>
        void continue_with(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC, ExceptHD&& exhD) noexcept
        {
            using ExceptA = first_arg_type<ExceptHA>;
            using ExceptB = first_arg_type<ExceptHB>;
            using ExceptC = first_arg_type<ExceptHC>;
            using ExceptD = first_arg_type<ExceptHD>;
            rpp::parallel_task([f=std::move(*this), move_args(task, exhA, exhB, exhC, exhD)]() mutable {
                try { f.get(); (void)task(); }
                catch (ExceptA& a) { (void)exhA(a); }
                catch (ExceptB& b) { (void)exhB(b); }
                catch (ExceptC& c) { (void)exhC(c); }
                catch (ExceptD& d) { (void)exhD(d); }
            });
        }

        /**
         * Abandons this future and prevents any waiting in destructor
         * @warning This will swallow any exceptions
         */
        void detach()
        {
            if (this->valid())
            {
                rpp::parallel_task([f=std::move(*this)]() mutable noexcept
                {
                    try { f.get(); }
                    catch (...) {}
                });
            }
        }

    #if RPP_HAS_COROUTINES // C++20 coroutine support
        // checks if the future is already finished
        bool await_ready() const noexcept
        {
            return this->wait_for(std::chrono::microseconds{0}) != std::future_status::timeout;
        }
        // suspension point that launches the background async task
        void await_suspend(RPP_CORO_STD::coroutine_handle<> cont) noexcept
        {
            rpp::parallel_task([this, cont]
            {
                if (this->valid())
                    this->wait();
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        void await_resume() const
        {
            // get() waits for the final result and will rethrow any exceptions from the cfuture
            this->get();
        }
    #endif
    }; // cfuture<void>


    /**
     * @brief Downcasts cfuture<T> into cfuture<void>, which allows to simply
     *        wait for a chain of futures to complete without getting a return value.
     * @code
     *     using namespace rpp::coro_operators;
     *     co_await async_task(operation1).then(operation2).then();
     * @endcode
     */
    template<typename T>
    cfuture<void> cfuture<T>::then() noexcept
    {
        return rpp::async_task([f=std::move(*this)]() mutable {
            (void)f.get();
        });
    }

    /** @brief Creates a future<T> which is already completed. Useful for some chaining edge cases. */
    template<typename T>
    cfuture<T> make_ready_future(T&& value)
    {
        std::promise<T> p;
        p.set_value(std::move(value));
        return p.get_future();
    }

    /** @brief Creates a future<void> which is already completed. Useful for some chaining edge cases. */
    inline cfuture<void> make_ready_future()
    {
        std::promise<void> p;
        p.set_value();
        return p.get_future();
    }

    /** @brief Creates a future<T> which is already errored with the exception. Useful for some chaining edge cases. */
    template<typename T, typename E>
    cfuture<T> make_exceptional_future(E&& e)
    {
        std::promise<T> p;
        p.set_exception(std::make_exception_ptr(std::forward<E>(e)));
        return p.get_future();
    }

    /** @brief Given a vector of futures, waits blockingly on all of them to be completed, without getting the values. */
    template<typename T>
    void wait_all(const std::vector<cfuture<T>>& vf)
    {
        for (const cfuture<T>& f : vf)
        {
            f.wait();
        }
    }

    /** @brief Blocks and gathers the results from all of the futures */
    template<typename T>
    std::vector<T> get_all(const std::vector<cfuture<T>>& vf)
    {
        std::vector<T> all;
        all.reserve(vf.size());
        for (const cfuture<T>& f : vf)
        {
            all.emplace_back(f.get());
        }
        return all;
    }

    /**
     * Used to launch multiple parallel Tasks and then gather
     * the results. It assumes that launcher already started its
     * own future task, which makes it more flexible.
     * @code
     *     std::vector<Result> results = rpp::get_tasks(dataList,
     *         [&](SomeData& data) {
     *             return rpp::async_task([&]{
     *                 return heavyComputation(data);
     *             };
     *         });
     * @endcode
     */
    template<typename U, typename Launcher>
    auto get_tasks(std::vector<U>& items, const Launcher& futureLauncher)
        -> std::vector< future_type<ret_type<Launcher>> >
    {
        using T = future_type<ret_type<Launcher>>;
        std::vector<cfuture<T>> futures;
        futures.reserve(items.size());

        for (U& item : items)
        {
            futures.emplace_back(futureLauncher(item));
        }

        return get_all(futures);
    }

    /**
     * Used to launch multiple parallel Tasks and then gather
     * the results. It uses rpp::async_task to launch the futures.
     * @code
     *     std::vector<Result> results = rpp::get_async_tasks(dataList,
     *         [&](SomeData& data) {
     *             return heavyComputation(data);
     *         });
     * @endcode
     */
    template<typename U, typename Callback>
    auto get_async_tasks(std::vector<U>& items, const Callback& futureCallback)
        -> std::vector< ret_type<Callback> >
    {
        using T = ret_type<Callback>;
        std::vector<cfuture<T>> futures;
        futures.reserve(items.size());

        for (U& item : items)
        {
            futures.emplace_back(rpp::async_task([&] {
                return futureCallback(item);
            }));
        }

        return get_all(futures);
    }

    /**
     * Used to launch multiple parallel Tasks and then wait for
     * the results. It assumes that launcher already started its
     * own future task, which makes it more flexible.
     * @code
     *     rpp::run_tasks(dataList, [&](SomeData& data) {
     *         return rpp::async_task([&]{
     *             heavyComputation(data);
     *         };
     *     });
     * @endcode
     */
    template<typename U, typename Launcher>
    void run_tasks(std::vector<U>& items, const Launcher& futureLauncher)
    {
        std::vector<cfuture<void>> futures;
        futures.reserve(items.size());

        for (U& item : items)
        {
            futures.emplace_back(futureLauncher(item));
        }

        return wait_all(futures);
    }
} // namespace rpp

#if RPP_HAS_COROUTINES
/**
 * Enable the use of rpp::cfuture<T> as a coroutine type
 * by using a rpp::cpromise<T> as the promise type.
 *
 * The most flexible way to do this is to define a specialization
 * of std::coroutine_traits<>
 *
 * This does not provide any async behaviors - simply allows easier interop
 * between existing rpp::cfuture<T> async functions
 *
 * @code
 *     // enables creating coroutines using rpp::cfuture<T>
 *     rpp::cfuture<std::string> doWorkAsync()
 *     {
 *         co_return "hello from suspendable method";
 *     }
 * @endcode
 */
template<typename T, typename... Args>
    requires(!std::is_void_v<T> && !std::is_reference_v<T>)
struct RPP_CORO_STD::coroutine_traits<rpp::cfuture<T>, Args...>
{
    struct promise_type : rpp::cpromise<T>
    {
        /**
         * For `rpp::cfuture<T> my_coroutine() {}` this is the hidden future object
         */
        rpp::cfuture<T> get_return_object() noexcept { return this->get_future().share(); }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() const noexcept { return {}; }
        void return_value(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>)
        {
            rpp::cpromise<T>::set_value(value);
        }
        void return_value(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>)
        {
            rpp::cpromise<T>::set_value(std::move(value));
        }
        void unhandled_exception() noexcept
        {
            rpp::cpromise<T>::set_exception(std::current_exception());
        }
    };
};

template<typename... Args>
struct RPP_CORO_STD::coroutine_traits<rpp::cfuture<void>, Args...>
{
    struct promise_type : rpp::cpromise<void>
    {
        rpp::cfuture<void> get_return_object() noexcept { return this->get_future().share(); }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_never final_suspend() const noexcept { return {}; }
        void return_void() noexcept { this->set_value(); }
        void unhandled_exception() noexcept { this->set_exception(std::current_exception()); }
    };
};

namespace rpp
{
    /**
     * @brief Allows to asynchronously co_await on any lambdas via rpp::parallel_task()
     * @code
     *     using namespace rpp::coro_operators;
     *     co_await [&]{ downloadFile(url); };
     * @endcode
     */
    template<typename Task>
        requires std::is_invocable_v<Task>
    struct lambda_awaiter
    {
        Task action;
        using T = decltype(action());
        T result {};
        rpp::pool_task* poolTask = nullptr;

        explicit lambda_awaiter(Task&& task) noexcept : action{std::move(task)} {}

        // is the task ready?
        bool await_ready() const noexcept
        {
            if (!poolTask) // task hasn't even been created yet!
                return false;
            return poolTask->wait(rpp::pool_task::duration{0}) != rpp::pool_task::wait_result::timeout;
        }
        // suspension point that launches the background async task
        void await_suspend(RPP_CORO_STD::coroutine_handle<> cont) noexcept
        {
            if (poolTask) std::terminate(); // avoid task explosion
            poolTask = rpp::parallel_task([this, cont]
            {
                result = action();
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        T await_resume() noexcept
        {
            return std::move(result);
        }
    };
    
    /**
     * @brief Awaiter object for std::chrono durations
     */
    template<class Clock = std::chrono::high_resolution_clock>
    struct chrono_awaiter
    {
        using time_point = typename Clock::time_point;
        using duration = typename Clock::duration;
        time_point end;

        explicit chrono_awaiter(const time_point& end) noexcept : end{end} {}
        explicit chrono_awaiter(const duration& d) noexcept : end{Clock::now() + d} {}

        template<typename Rep, typename Period>
        explicit chrono_awaiter(std::chrono::duration<Rep, Period> d) noexcept
            : end{Clock::now() + std::chrono::duration_cast<duration>(d)} {}

        bool await_ready() const noexcept
        {
            return Clock::now() >= end;
        }
        void await_suspend(RPP_CORO_STD::coroutine_handle<> cont) const
        {
            rpp::parallel_task([cont,end=end]
            {
                // using cv wait_until since that's the most accurate way to suspend on both unix and win32
                // only problem is spurious wakeups, but a simple while loop takes care of that
                {
                    rpp::condition_variable cv;
                    std::mutex m;
                    std::unique_lock lock { m };
                    while (cv.wait_until(lock, end) != std::cv_status::timeout)
                    {
                    }
                }
                // resume execution while still inside the background thread
                cont.resume();
            });
        }
        void await_resume() const noexcept {}
    };

    inline namespace coro_operators
    {
        /**
         * @brief Allows to asynchronously co_await on any lambdas via rpp::parallel_task()
         * @code
         *     using namespace rpp::coro_operators;
         *     co_await [&]{ downloadFile(url); };
         * @endcode
         */
        template<typename Task>
            requires std::is_invocable_v<Task>
        lambda_awaiter<Task> operator co_await(Task&& task) noexcept
        {
            return lambda_awaiter<Task>{ std::move(task) };
        }

        /**
         * @brief Allows to co_await on a std::chrono::duration
         * @code
         *     using namespace rpp::coro_operators;
         *     co_await std::chrono::milliseconds{100};
         * @endcode
         */
        template<typename Rep, typename Period>
        auto operator co_await(const std::chrono::duration<Rep, Period>& duration) noexcept
        {
            return chrono_awaiter<>{ duration };
        }
    }
} // namespace rpp coro
#endif // Coroutines support for C++20

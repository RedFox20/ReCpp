#pragma once
/**
 * Chainable and Coroutine compatible std::future extensions
 * Copyright (c) 2017-2018, 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "thread_pool.h"
#include "future_types.h"
#include "traits.h"
#include "debugging.h" // __assertion_failure

namespace rpp
{
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
        cfuture<T> f = p.get_future();
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
    class NODISCARD cfuture : public std::future<T>
    {
    public:
        using super = std::future<T>;
        using value_type = T;
        cfuture() noexcept = default;
        cfuture(std::future<T>&& f) noexcept : super{std::move(f)} {}
        cfuture(cfuture&& f)        noexcept : super{std::move(f)} {}
        cfuture& operator=(std::future<T>&& f) noexcept { super::operator=(std::move(f)); return *this; }
        cfuture& operator=(cfuture&& f)        noexcept { super::operator=(std::move(f)); return *this; }

        /**
         * @warning If the future is not waited before destruction, program will std::terminate()
         */
        ~cfuture() noexcept
        {
            if (this->valid())
            {
                // check if we have waited for the future before destruction
                if (await_ready())
                {
                    try { (void)this->get(); }
                    catch (std::exception& e) { __assertion_failure("cfuture<T> threw exception in destructor: %s", e.what()); }
                }
                // NOTE: This is a fail-fast strategy to catch programming bugs
                //       and this happens if you forget to await a future before destruction.
                //       The default behavior of std::future<T> is to silently block in the destructor,
                //       however in ReCpp we terminate immediately to force the programmer to fix the bug.
                //       https://stackoverflow.com/questions/23455104/why-is-the-destructor-of-a-future-returned-from-stdasync-blocking
                __assertion_failure("cfuture<T> was not awaited before destruction");
                std::terminate(); // always terminate
            }
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
        auto then(Task&& task) noexcept -> cfuture<decltype(task(this->get()))>
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
        auto then(Task&& task, ExceptHA&& exhA) noexcept -> cfuture<decltype(task(this->get()))>
        {
            using ExceptA = first_arg_type<ExceptHA>;
            return async_task([f=std::move(*this), move_args(task, exhA)]() mutable {
                try { return task(f.get()); } 
                catch (ExceptA& a) { return exhA(a); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB>
        auto then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB) noexcept -> cfuture<decltype(task(this->get()))>
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
        auto then(Task task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC) noexcept -> cfuture<decltype(task(this->get()))>
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
        auto then(Task task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC, ExceptHD&& exhD) noexcept -> cfuture<decltype(task(this->get()))>
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

        template<typename U>
        auto then(cfuture<U>&& next) noexcept -> cfuture<U>
        {
            return rpp::async_task([f=std::move(*this), n=std::move(next)]() mutable {
                { (void)f.get(); } // wait for this future to complete
                return n.get(); // return the result of the next future
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

        // checks if the future is already finished
        bool await_ready() const noexcept
        {
            return this->wait_for(std::chrono::microseconds{0}) != std::future_status::timeout;
        }

    #if RPP_HAS_COROUTINES // C++20 coro support

        // suspension point that launches the background async task
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            rpp::parallel_task([this, cont]() /*clang-12 compat:*/mutable
            {
                if (this->valid())
                    this->wait();
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }

        /**
         * @returns The result and will rethrow any exceptions from the cfuture
         */
        [[nodiscard]] inline T await_resume()
        {
            return super::get();
        }

        /**
         * Enable the use of rpp::cfuture<T> as a coroutine type
         * by using a rpp::cpromise<T> as the promise type.
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
        struct promise_type : rpp::cpromise<T>
        {
            /**
             * For `rpp::cfuture<T> my_coroutine() {}` this is the hidden future object
             */
            rpp::cfuture<T> get_return_object() noexcept { return this->get_future(); }
            RPP_CORO_STD::suspend_never initial_suspend() const noexcept { return {}; }
            RPP_CORO_STD::suspend_never final_suspend() const noexcept { return {}; }
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
    #endif // RPP_HAS_COROUTINES
    }; // cfuture<T>


    /**
     * Composable Futures with Coroutine support. See docs in cfuture<T>.
     */
    template<>
    class NODISCARD cfuture<void> : public std::future<void>
    {
    public:
        using super = future<void>;
        using value_type = void;
        cfuture() noexcept = default;
        cfuture(std::future<void>&& f) noexcept : super{std::move(f)} {}
        cfuture(cfuture&& f)           noexcept : super{std::move(f)} {}
        cfuture& operator=(std::future<void>&& f) noexcept { super::operator=(std::move(f)); return *this; }
        cfuture& operator=(cfuture&& f)           noexcept { super::operator=(std::move(f)); return *this; }

        /**
         * @warning If the future is not waited before destruction, program will std::terminate()
         */
        ~cfuture() noexcept
        {
            if (this->valid())
            {
                // check if we have waited for the future before destruction
                if (await_ready())
                {
                    try { (void)this->get(); }
                    catch (std::exception& e) { __assertion_failure("cfuture<void> threw exception in destructor: %s", e.what()); }
                }
                // NOTE: This is a fail-fast strategy to catch programming bugs
                //       and this happens if you forget to await a future before destruction.
                //       The default behavior of std::future<T> is to silently block in the destructor,
                //       however in ReCpp we terminate immediately to force the programmer to fix the bug.
                //       https://stackoverflow.com/questions/23455104/why-is-the-destructor-of-a-future-returned-from-stdasync-blocking
                __assertion_failure("cfuture<void> was not awaited before destruction");
                std::terminate(); // always terminate
            }
        }

        /**
         * @brief This is a no-op on cfuture<void>
         */
        cfuture<void> then() noexcept { return { std::move(*this) }; }

        /**
         * Standard continuation of a void future:
         * connectToServer(auth).then([=] {
         *     auto r = sendUserCredentials();
         *     return getLoginResponse(r);
         * });
         */
        template<typename Task>
        auto then(Task&& task) noexcept -> cfuture<decltype(task())>
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
        auto then(Task&& task, ExceptHA&& exhA) noexcept -> cfuture<decltype(task())>
        {
            using ExceptA = first_arg_type<ExceptHA>;
            return async_task([f=std::move(*this), move_args(task, exhA)]() mutable {
                try { f.get(); return task(); }
                catch (ExceptA& a) { return exhA(a); }
            });
        }

        template<typename Task, class ExceptHA, class ExceptHB>
        auto then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB) noexcept -> cfuture<decltype(task())>
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
        auto then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC) noexcept -> cfuture<decltype(task())>
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
        auto then(Task&& task, ExceptHA&& exhA, ExceptHB&& exhB, ExceptHC&& exhC, ExceptHD&& exhD) noexcept -> cfuture<decltype(task())>
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

        template<typename U>
        auto then(cfuture<U>&& next) noexcept -> cfuture<U>
        {
            return rpp::async_task([f=std::move(*this), n=std::move(next)]() mutable {
                { f.get(); } // wait for this future to complete
                return n.get(); // return the result of the next future
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

        // checks if the future is already finished
        bool await_ready() const noexcept
        {
            return this->wait_for(std::chrono::microseconds{0}) != std::future_status::timeout;
        }

    #if RPP_HAS_COROUTINES // C++20 coroutine support

        // suspension point that launches the background async task
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            rpp::parallel_task([this, cont]() /*clang-12 compat:*/mutable
            {
                if (this->valid())
                    this->wait();
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }

        /**
         * @briefWaits for the final result and will rethrow any exceptions from the cfuture
         */
        inline void await_resume()
        {
            this->get();
        }

        /**
         * Enable the use of rpp::cfuture<void> as a coroutine type.
         *
         * This does not provide any async behaviors - simply allows easier interop
         * between existing rpp::cfuture<void> async functions
         *
         * @code
         *     rpp::cfuture<void> doWorkAsync()
         *     {
         *         co_return;
         *     }
         * @endcode
         */
        struct promise_type : rpp::cpromise<void>
        {
            rpp::cfuture<void> get_return_object() noexcept { return this->get_future(); }
            RPP_CORO_STD::suspend_never initial_suspend() const noexcept { return {}; }
            RPP_CORO_STD::suspend_never final_suspend() const noexcept { return {}; }
            void return_void() noexcept { this->set_value(); }
            void unhandled_exception() noexcept { this->set_exception(std::current_exception()); }
        };
    #endif // RPP_HAS_COROUTINES
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
    inline cfuture<void> cfuture<T>::then() noexcept
    {
        return rpp::async_task([f=std::move(*this)]() mutable {
            (void)f.get();
        });
    }

    /** @brief Creates a future<T> which is already completed. Useful for some chaining edge cases. */
    template<typename T>
    inline cfuture<T> make_ready_future(T&& value)
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
    inline cfuture<T> make_exceptional_future(E&& e)
    {
        std::promise<T> p;
        p.set_exception(std::make_exception_ptr(std::forward<E>(e)));
        return p.get_future();
    }

    /** @brief Given a vector of futures, waits blockingly on all of them to be completed, without getting the values. */
    template<typename T>
    inline void wait_all(const std::vector<cfuture<T>>& vf)
    {
        for (const cfuture<T>& f : vf)
        {
            f.wait();
        }
    }

    /** @brief Blocks and gathers the results from all of the futures */
    template<typename T>
    inline std::vector<T> get_all(std::vector<cfuture<T>>& vf)
    {
        std::vector<T> all;
        all.reserve(vf.size());
        for (cfuture<T>& f : vf)
        {
            all.emplace_back(f.get());
        }
        return all;
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
    inline void run_tasks(std::vector<U>& items, const Launcher& futureLauncher)
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

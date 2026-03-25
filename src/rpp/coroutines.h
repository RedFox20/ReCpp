#pragma once
/**
 * C++20 Coroutine facilities, Copyright (c) 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include "future.h"
#include "timer.h" // rpp::Duration

#if RPP_HAS_COROUTINES

namespace rpp
{
    /**
     * @brief Allows to asynchronously co_await on any lambdas via rpp::parallel_task()
     * @code
     *     using namespace rpp::coro_operators;
     *     co_await [&]{ downloadFile(url); };
     * @endcode
     * A more complex example with nested coroutines:
     * @code
     *     using namespace rpp::coro_operators;
     *     co_await [&]() -> cfuture<string> {
     *         std::string localPath = co_await downloadFile(url);
     *         co_return localPath;
     *     };
     * @endcode
     */
    template<typename T>
    struct RPP_CORO_RETURN_TYPE functor_awaiter
    {
        rpp::delegate<T()> action;
        T result {};
        std::exception_ptr ex {};
        rpp::pool_task_handle poolTask {nullptr};

        explicit functor_awaiter(rpp::delegate<T()> action) noexcept
            : action{std::move(action)} {}

        // Called first by the compiler immediately after `co_await action` is evaluated.
        // If true, the coroutine skips suspension entirely and jumps straight to await_resume().
        // Since poolTask hasn't been started yet (that happens in await_suspend), this always
        // returns false, guaranteeing the coroutine suspends and the background task is launched.
        bool await_ready() const noexcept
        {
            return poolTask.was_started() && poolTask.is_finished();
        }
        // Called after await_ready() returns false, just as the coroutine is suspending.
        // `cont` is a handle to the now-suspended coroutine; resuming it continues execution
        // past the co_await expression and calls await_resume() on this background thread.
        // The caller (the co_await site) returns here, giving control back up the call stack.
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (poolTask.was_started()) std::terminate(); // avoid task explosion
            poolTask = rpp::parallel_task([this, cont]() /*clang-12 compat*/mutable
            {
                try { result = std::move(action()); }
                catch (...) { ex = std::current_exception(); }
                // WARNING: do not deallocate action here, it can lead to a race-condition + memory corruption
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        // Called by the compiler on the background thread immediately after cont.resume() returns
        // control to the coroutine. At this point the background task has fully completed and
        // result/ex are written. Returns the result to the co_await expression, or rethrows.
        T await_resume()
        {
            // dtor consistency test: clear some of the state stuff here
            action = {};
            poolTask = {nullptr};
            if (ex) std::rethrow_exception(ex);
            return std::move(result);
        }
    };

    template<>
    struct RPP_CORO_RETURN_TYPE functor_awaiter<void>
    {
        rpp::delegate<void()> action;

        std::exception_ptr ex {};
        rpp::pool_task_handle poolTask {nullptr};

        explicit functor_awaiter(rpp::delegate<void()> action) noexcept
            : action{std::move(action)} {}

        // is the task ready?
        bool await_ready() const noexcept
        {
            return poolTask.was_started() && poolTask.is_finished();
        }
        // suspension point that launches the background async task
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (poolTask.was_started()) std::terminate(); // avoid task explosion
            poolTask = rpp::parallel_task([this, cont]() /*clang-12 compat*/mutable {
                try { action(); }
                catch (...) { ex = std::current_exception(); }
                // WARNING: do not deallocate action here, it can lead to a race-condition + memory corruption
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        // similar to future<void>, rethrows the caught exception or does nothing
        void await_resume()
        {
            // dtor consistency test: clear some of the state stuff here
            action = {};
            poolTask = {nullptr};
            if (ex) std::rethrow_exception(ex);
        }
    };

    template<IsFuture Future>
    struct RPP_CORO_RETURN_TYPE functor_awaiter_fut
    {
        rpp::delegate<Future()> action;
        Future f {};
        std::exception_ptr ex {};
        rpp::pool_task_handle poolTask {nullptr};

        explicit functor_awaiter_fut(rpp::delegate<Future()> action) noexcept
            : action{std::move(action)} {}

        // is the task ready?
        bool await_ready() const noexcept
        {
            return poolTask.was_started() && poolTask.is_finished();
        }
        // suspension point that launches the background async task
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (poolTask.was_started()) std::terminate(); // avoid task explosion
            poolTask = rpp::parallel_task([this, cont]() /*clang-12 compat*/mutable
            {
                try {
                    f = action(); // get the future from the lambda
                    f.wait(); // wait for the nested coroutine to finish (can throw)
                } catch (...) { ex = std::current_exception(); }
                // WARNING: do not deallocate action here, it can lead to a race-condition + memory corruption
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        // similar to future<T>, either gets the result T, or throws the caught exception
        auto await_resume()
        {
            // dtor consistency test: clear some of the state stuff here
            action = {};
            poolTask = {nullptr};
            if (ex) std::rethrow_exception(ex);
            return f.get();
        }
    };

    // TODO: should reimplement this using coroutine traits
    // TODO: https://en.cppreference.com/w/cpp/coroutine/coroutine_traits
    template<typename T>
    struct RPP_CORO_RETURN_TYPE std_future_awaiter
    {
        std::future<T> f;
        std::exception_ptr ex {};
        rpp::pool_task_handle poolTask {nullptr};

        // NOTE: there is no safe way to grab the reference normally
        //       so we always MOVE the future into this awaiter
        explicit std_future_awaiter(std::future<T>&& f) noexcept
            : f{std::move(f)} {}

        // is the task ready?
        bool await_ready() const noexcept
        {
            return poolTask.was_started() && poolTask.is_finished();
        }
        // suspension point that launches the background async task
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (poolTask.was_started()) std::terminate(); // avoid task explosion
            poolTask = rpp::parallel_task([this, cont]() /*clang-12 compat*/mutable
            {
                try { f.wait(); /* wait for the nested coroutine to finish (can throw) */ }
                catch (...) { ex = std::current_exception(); }
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        // similar to future<T>, either gets the result T, or throws the caught exception
        T await_resume()
        {
            if (ex) std::rethrow_exception(ex);
            return f.get();
        }
    };

    /**
     * @brief Awaiter object for std::chrono durations
     */
    struct RPP_CORO_RETURN_TYPE time_awaiter
    {
        rpp::TimePoint end;

        explicit time_awaiter(const rpp::TimePoint& end) noexcept : end{end} {}
        explicit time_awaiter(const rpp::Duration& d) noexcept : end{rpp::TimePoint::now() + d} {}
        bool await_ready() const noexcept
        {
            return rpp::TimePoint::now() >= end;
        }
        void await_suspend(rpp::coro_handle<> cont) const
        {
            rpp::parallel_task_detached([cont,end=end]() /*clang-12 compat*/mutable
            {
                rpp::sleep_until(end);
                // resume execution while still inside the background thread
                cont.resume();
            });
        }
        void await_resume() const noexcept {}
    };


    inline namespace coro_operators
    {
        /**
         * @brief Allows to asynchronously co_await on any delegate via rpp::parallel_task()
         * @note This overload is for delegates that dont return a future
         * @code
         *     using namespace rpp::coro_operators;
         *     rpp::delegate<string()> action = [&]{ return downloadFile(url); };
         *     string localPath = co_await action;
         * @endcode
         */
        template<NotFuture T>
        RPP_CORO_WRAPPER inline functor_awaiter<T> operator co_await(rpp::delegate<T()>&& action) noexcept
        {
            return functor_awaiter<T>{ std::move(action) };
        }

        /**
         * @brief Allows to co_await on any delegate which returns a future
         * @note The delegate must return a future type
         * @code
         *     using namespace rpp::coro_operators;
         *     rpp::delegate<cfuture<string>()> action = [&]() -> cfuture<string> {
         *         string localPath = co_await downloadFile(url);
         *         co_return localPath;
         *     };
         *     string path = co_await action;
         * @endcode
         */
        template<IsFuture F>
        RPP_CORO_WRAPPER inline functor_awaiter_fut<F> operator co_await(rpp::delegate<F()>&& action) noexcept
        {
            return functor_awaiter_fut<F>{ std::move(action) };
        }

        /**
         * @brief Allows to asynchronously co_await on any lambdas via rpp::parallel_task()
         * @note This overload is for functions that dont return a future
         * @code
         *     using namespace rpp::coro_operators;
         *     co_await [&]{ downloadFile(url); };
         * @endcode
         */
        template<IsFunctionNotReturningFuture F>
        RPP_CORO_WRAPPER inline auto operator co_await(F&& action) noexcept -> functor_awaiter<decltype(action())>
        {
            return functor_awaiter<decltype(action())>{ std::move(action) };
        }

        /**
         * @brief Allows to co_await on a lambda which returns a future
         * @note The lambda must return a future type
         * @code
         *     using namespace rpp::coro_operators;
         *     string path = co_await [&]() -> cfuture<string> {
         *         string localPath = co_await downloadFile(url);
         *         co_return localPath;
         *     };
         * @endcode
         */
        template<IsFunctionReturningFuture FF>
        RPP_CORO_WRAPPER inline auto operator co_await(FF&& action) noexcept -> functor_awaiter_fut<decltype(action())>
        {
            return functor_awaiter_fut<decltype(action())>{ std::move(action) };
        }

        template<typename T>
        inline rpp::cfuture<T>& operator co_await(rpp::cfuture<T>& future) noexcept
        {
            return future;
        }

        template<typename T>
        rpp::cfuture<T>&& operator co_await(rpp::cfuture<T>&& future) noexcept
        {
            return (rpp::cfuture<T>&&)future;
        }

        template<typename T>
        RPP_CORO_WRAPPER inline std_future_awaiter<T> operator co_await(std::future<T>& future) noexcept
        {
            return std_future_awaiter<T>{ std::move(future) };
        }

        template<typename T>
        RPP_CORO_WRAPPER inline std_future_awaiter<T> operator co_await(std::future<T>&& future) noexcept
        {
            return std_future_awaiter<T>{ std::move(future) };
        }

        /**
         * @brief Allows to co_await on a std::chrono::duration
         * @code
         *     using namespace rpp::coro_operators;
         *     co_await rpp::millis(100);
         * @endcode
         */
        RPP_CORO_WRAPPER inline auto operator co_await(const rpp::Duration& duration) noexcept
        {
            return time_awaiter{ duration };
        }
    }
} // namespace rpp
#endif // Coroutines support for C++20

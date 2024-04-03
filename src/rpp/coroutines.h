#pragma once
/**
 * C++20 Coroutine facilities, Copyright (c) 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include "thread_pool.h"
#include "debugging.h"
#include "traits.h"
#include "future_types.h"

#if RPP_HAS_COROUTINES
#include <chrono>
#if _MSC_VER
#  include "timer.h"
#endif

namespace rpp
{
    template<typename F>
    concept IsFunctionReturningFuture = requires(F f)
    {
        requires IsFunction<F> && IsFuture<decltype(f())>;
    };

    template<typename F>
    concept IsFunctionNotReturningFuture = requires(F f)
    {
        requires IsFunction<F> && NotFuture<decltype(f())>;
    };

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
    struct functor_awaiter
    {
        rpp::delegate<T()> action;
        T result {};
        std::exception_ptr ex {};
        rpp::pool_task_handle poolTask {nullptr};

        explicit functor_awaiter(rpp::delegate<T()>&& action) noexcept
            : action{std::move(action)} {}

        // is the task ready?
        bool await_ready() const noexcept
        {
            return poolTask && poolTask.wait_check();
        }
        // suspension point that launches the background async task
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (poolTask) std::terminate(); // avoid task explosion
            poolTask = rpp::parallel_task([this, cont]() /*clang-12 compat*/mutable
            {
                try { result = std::move(action()); }
                catch (...) { ex = std::current_exception(); }
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        // similar to future<T>, either gets the result T, or throws the caught exception
        T await_resume()
        {
            if (ex) std::rethrow_exception(ex);
            return std::move(result);
        }
    };

    template<>
    struct functor_awaiter<void>
    {
        rpp::delegate<void()> action;

        std::exception_ptr ex {};
        rpp::pool_task_handle poolTask {nullptr};

        explicit functor_awaiter(rpp::delegate<void()>&& action) noexcept
            : action{std::move(action)} {}

        // is the task ready?
        bool await_ready() const noexcept
        {
            return poolTask && poolTask.wait_check();
        }
        // suspension point that launches the background async task
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (poolTask) std::terminate(); // avoid task explosion
            poolTask = rpp::parallel_task([this, cont]() /*clang-12 compat*/mutable {
                try { action(); }
                catch (...) { ex = std::current_exception(); }
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        // similar to future<void>, rethrows the caught exception or does nothing
        void await_resume()
        {
            if (ex) std::rethrow_exception(ex);
        }
    };

    template<IsFuture Future>
    struct functor_awaiter_fut
    {
        rpp::delegate<Future()> action;
        Future f {};
        std::exception_ptr ex {};
        rpp::pool_task_handle poolTask {nullptr};

        explicit functor_awaiter_fut(rpp::delegate<Future()>&& action) noexcept
            : action{std::move(action)} {}

        // is the task ready?
        bool await_ready() const noexcept
        {
            return poolTask && poolTask.wait_check();
        }
        // suspension point that launches the background async task
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (poolTask) std::terminate(); // avoid task explosion
            poolTask = rpp::parallel_task([this, cont]() /*clang-12 compat*/mutable
            {
                try {
                    f = action(); // get the future from the lambda
                    f.wait(); // wait for the nested coroutine to finish (can throw)
                } catch (...) { ex = std::current_exception(); }
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        // similar to future<T>, either gets the result T, or throws the caught exception
        auto await_resume()
        {
            if (ex) std::rethrow_exception(ex);
            return f.get();
        }
    };

    // TODO: should reimplement this using coroutine traits
    // TODO: https://en.cppreference.com/w/cpp/coroutine/coroutine_traits
    template<typename T>
    struct std_future_awaiter
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
            return poolTask && poolTask.wait_check();
        }
        // suspension point that launches the background async task
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (poolTask) std::terminate(); // avoid task explosion
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
        void await_suspend(rpp::coro_handle<> cont) const
        {
            rpp::parallel_task([cont,end=end]() /*clang-12 compat*/mutable
            {
            #if _MSC_VER
                // win32 internal Sleep is incredibly inaccurate, so rpp::sleep_us must be used
                auto d = std::chrono::duration_cast<std::chrono::microseconds>(end - Clock::now());
                int micros = static_cast<int>(d.count());
                if (micros > 0)
                {
                    rpp::sleep_us(micros);
                }
            #else
                std::this_thread::sleep_until(end);
            #endif
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
        inline functor_awaiter<T> operator co_await(rpp::delegate<T()>&& action) noexcept
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
        inline functor_awaiter_fut<F> operator co_await(rpp::delegate<F()>&& action) noexcept
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
        inline auto operator co_await(F&& action) noexcept -> functor_awaiter<decltype(action())>
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
        inline auto operator co_await(FF&& action) noexcept -> functor_awaiter_fut<decltype(action())>
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
        inline std_future_awaiter<T> operator co_await(std::future<T>& future) noexcept
        {
            return std_future_awaiter<T>{ std::move(future) };
        }

        template<typename T>
        inline std_future_awaiter<T> operator co_await(std::future<T>&& future) noexcept
        {
            return std_future_awaiter<T>{ std::move(future) };
        }

        /**
         * @brief Allows to co_await on a std::chrono::duration
         * @code
         *     using namespace rpp::coro_operators;
         *     co_await std::chrono::milliseconds{100};
         * @endcode
         */
        template<typename Rep, typename Period>
        inline auto operator co_await(const std::chrono::duration<Rep, Period>& duration) noexcept
        {
            return chrono_awaiter<>{ duration };
        }
    }
} // namespace rpp
#endif // Coroutines support for C++20

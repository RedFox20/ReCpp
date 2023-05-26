#pragma once
/**
 * C++20 Coroutine facilities, Copyright (c) 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include "thread_pool.h"

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

#if RPP_HAS_COROUTINES
#include <chrono>
#if _MSC_VER
#  include "timer.h"
#endif

namespace rpp
{
    template<typename T = void>
    using coro_handle = RPP_CORO_STD::coroutine_handle<T>;


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

        std::conditional_t<std::is_same_v<T, void>, void*, T> result {};
        std::exception_ptr ex {};
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
        void await_suspend(rpp::coro_handle<> cont) noexcept
        {
            if (poolTask) std::terminate(); // avoid task explosion
            poolTask = rpp::parallel_task([this, cont]() /*clang-12 compat*/mutable
            {
                try
                {
                    if constexpr (!std::is_same_v<T, void>)
                    {
                        result = std::move(action());
                    }
                    else
                    {
                        action();
                    }
                }
                catch (...)
                {
                    ex = std::current_exception();
                }
                cont.resume(); // call await_resume() and continue on this background thread
            });
        }
        // similar to future<T>, either gets the result T, or throws the caught exception
        T await_resume()
        {
            if (ex)
            {
                std::rethrow_exception(ex);
            }
            if constexpr (!std::is_same_v<T, void>)
            {
                return std::move(result);
            }
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
} // namespace rpp
#endif // Coroutines support for C++20

#pragma once
/**
 * C++20 Coroutine facilities, Copyright (c) 2024, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include <future>

#if RPP_HAS_CXX20 && defined(__has_include) // Coroutines support for C++20
    #if __has_include(<coroutine>)
        #include <coroutine>
        #define RPP_HAS_COROUTINES 1
        #define RPP_CORO_STD std
    #elif __has_include(<experimental/coroutine>) // backwards compatibility for clang-14 and older
        #include <experimental/coroutine>
        #define RPP_HAS_COROUTINES 1
        #define RPP_CORO_STD std::experimental
        // Clang 14 warns about std::experimental coroutines being removed in LLVM 15,
        // suppress globally since it triggers at every co_await/co_return usage site
        #if defined(__clang__) && defined(__has_warning)
            #if __has_warning("-Wdeprecated-experimental-coroutine")
                #pragma clang diagnostic ignored "-Wdeprecated-experimental-coroutine"
            #endif
        #endif
    #else
        #define RPP_HAS_COROUTINES 0
        #define RPP_CORO_STD
    #endif
#else // no coroutine support for C++17 and below
    #define RPP_HAS_COROUTINES 0
    #define RPP_CORO_STD
#endif

namespace rpp
{
    template<class T = void>
    class NODISCARD RPP_CORO_RETURN_TYPE RPP_CORO_LIFETIMEBOUND cfuture;


#if RPP_HAS_COROUTINES
    template<typename T = void>
    using coro_handle = RPP_CORO_STD::coroutine_handle<T>;
#endif // RPP_HAS_COROUTINES


#if RPP_HAS_CXX20

    template<typename F>
    concept IsFuture = requires(F f) {
        requires std::is_same_v<F, rpp::cfuture<decltype(f.get())>>
              || std::is_same_v<F, std::future<decltype(f.get())>>;
    };

    template<typename F>
    concept NotFuture = !IsFuture<F>;

    template<typename Function>
    concept IsFunction = std::is_invocable_v<Function>;

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

#endif // RPP_HAS_CXX20
}

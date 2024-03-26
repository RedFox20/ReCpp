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
    template<typename T = void>
    using coro_handle = RPP_CORO_STD::coroutine_handle<T>;

    template<class T = void>
    class NODISCARD cfuture;

#if RPP_HAS_CXX20
    template<typename F>
    concept IsFuture = requires(F f) {
        requires std::is_same_v<F, rpp::cfuture<decltype(f.get())>>
              || std::is_same_v<F, std::future<decltype(f.get())>>;
    };

    template<typename F>
    concept NotFuture = requires(F f) {
        requires !std::is_same_v<F, rpp::cfuture<decltype(f.get())>>
              && !std::is_same_v<F, std::future<decltype(f.get())>>;
    };
#endif
}

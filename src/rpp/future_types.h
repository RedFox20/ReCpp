#pragma once
/**
 * C++20 Coroutine facilities, Copyright (c) 2024, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include <future>
// C++20 makes <coroutine> mandatory, freestanding included, and config.h asserts C++20
#include <coroutine>

namespace rpp
{
    template<class T = void>
    class NODISCARD RPP_CORO_RETURN_TYPE RPP_CORO_LIFETIMEBOUND cfuture;


    template<typename T = void>
    using coro_handle = std::coroutine_handle<T>;
    using suspend_never = std::suspend_never;
    using suspend_always = std::suspend_always;


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
}

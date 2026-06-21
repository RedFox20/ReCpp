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
        #ifdef _VSTD_EXPERIMENTAL
            #define RPP_CORO_STD _VSTD_EXPERIMENTAL
        #else
            #define RPP_CORO_STD std::experimental
        #endif
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
    using suspend_never = RPP_CORO_STD::suspend_never;
    using suspend_always = RPP_CORO_STD::suspend_always;

    namespace detail
    {
        // Set by event_loop on its owner thread; captured by the awaiter of a task/cfuture so the
        // continuation is posted back to the owner loop instead of resuming on whatever pool thread
        // completed the awaited work. Keeps event-loop-bound coroutines loop-affine across co_await.
        using loop_post_fn = void (*)(void* ctx, rpp::coro_handle<>) noexcept;
        struct loop_ctx
        {
            loop_post_fn fn = nullptr;
            void* ctx = nullptr;
            // Resume the continuation on the owner loop if there is one, else inline on this thread.
            void resume(rpp::coro_handle<> cont) const noexcept { fn ? fn(ctx, cont) : cont.resume(); }
        };
        inline thread_local loop_ctx tl_loop;
    }
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

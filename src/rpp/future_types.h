#pragma once
/**
 * C++20 Coroutine facilities, Copyright (c) 2024, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include <type_traits>
// C++20 makes <coroutine> mandatory, freestanding included, and config.h asserts C++20
#include <coroutine>
// This header must never reach <future>. gcc-14 crashes any importer of a module whose
// fragment carries it, and every header which includes this one would carry it. See BUGS.md B16

namespace rpp
{
    template<class T = void>
    class NODISCARD RPP_CORO_RETURN_TYPE RPP_CORO_LIFETIMEBOUND cfuture;


    template<typename T = void>
    using coro_handle = std::coroutine_handle<T>;
    using suspend_never = std::suspend_never;
    using suspend_always = std::suspend_always;


    template<typename Function>
    concept IsFunction = std::is_invocable_v<Function>;

    // IsFuture and the concepts built on it live in future.h, which owns <future>
}

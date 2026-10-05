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

// rpp::future in rpp/async.h replaces cfuture, see docs/FUTURE_MIGRATION.md section 6.1
#define RPP_DEPRECATED_CFUTURE [[deprecated("use rpp::future, rpp::promise and rpp::async from rpp/async.h")]]

namespace rpp
{
    // a gcc importer which includes this header first drops the attribute, unless this declaration has it too
    template<class T = void>
    class RPP_DEPRECATED_CFUTURE NODISCARD RPP_CORO_RETURN_TYPE RPP_CORO_LIFETIMEBOUND cfuture;


    /// The outcome of a timed wait on a task or a future
    enum class wait_result : int
    {
        finished, // the result is ready, and it can hold an exception
        timeout,  // waiting on task timed out
        deferred, // the task runs on get(), so no wait can finish it
    };


    template<typename T = void>
    using coro_handle = std::coroutine_handle<T>;
    using suspend_never = std::suspend_never;
    using suspend_always = std::suspend_always;


    template<typename Function>
    concept IsFunction = std::is_invocable_v<Function>;

    // IsFuture and the concepts built on it live in future.h, which owns <future>
}

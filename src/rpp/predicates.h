/**
 * C++20 predicate / invocable concepts
 */
#pragma once
#include "config.h"

namespace rpp
{

#if RPP_HAS_CXX20
    // Portable replacement for std::invocable which requires <concepts>
    // and is not available on all platforms (e.g. Android NDK r25b / Clang 14)
    // Uses requires-expression instead of <type_traits> to avoid the dependency
    template<typename Callable>
    concept IsCallable = requires(Callable&& c) { c(); };

    // Portable replacement for std::predicate which requires <concepts>
    // and is not yet available on all platforms
    template<typename Predicate>
    concept IsPredicate = requires(Predicate&& p) { static_cast<bool>(p()); };
#else
    #define IsCallable typename
    #define IsPredicate typename
#endif

}
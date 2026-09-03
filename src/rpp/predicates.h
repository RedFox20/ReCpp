/**
 * C++20 predicate / invocable concepts
 */
#pragma once
#include "config.h"

namespace rpp
{

    // a requires-expression replaces std::invocable, which needs <concepts>
    template<typename Callable>
    concept IsCallable = requires(Callable&& c) { c(); };

    // a requires-expression replaces std::predicate, which needs <concepts>
    template<typename Predicate>
    concept IsPredicate = requires(Predicate&& p) { static_cast<bool>(p()); };

}
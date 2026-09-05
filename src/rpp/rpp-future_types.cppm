// C++20 module interface unit for <rpp/future_types.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "future_types.h"

export module rpp.future_types;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;

export namespace rpp {
    using rpp::cfuture;
    using rpp::coro_handle;
    using rpp::suspend_never;
    using rpp::suspend_always;
    using rpp::IsFuture;
    using rpp::NotFuture;
    using rpp::IsFunction;
    using rpp::IsFunctionReturningFuture;
    using rpp::IsFunctionNotReturningFuture;
}
// GENERATED EXPORTS END

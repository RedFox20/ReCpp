// C++20 module interface unit for <rpp/coroutines.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "coroutines.h"

export module rpp.coroutines;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::functor_awaiter;
    using rpp::functor_awaiter_fut;
    using rpp::std_future_awaiter;
    using rpp::time_awaiter;
}

export namespace rpp::inline coro_operators {
    using rpp::coro_operators::operator co_await;
}
// GENERATED EXPORTS END

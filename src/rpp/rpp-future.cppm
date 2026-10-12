// C++20 module interface unit for the rpp.future headers, owned by tools/gen_module_exports.py.
// The headers stay in the global module fragment, so an importer and an includer share one entity.
module;

#include "event_loop.h"
#include "coroutines.h"

export module rpp.future;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::event_task;
    using rpp::event_loop;
    using rpp::functor_awaiter;
    using rpp::functor_awaiter_fut;
    using rpp::time_awaiter;
}

export namespace rpp::inline coro_operators {
    using rpp::coro_operators::operator co_await;
}
// GENERATED EXPORTS END

// C++20 module interface unit for <rpp/task.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "task.h"

export module rpp.task;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
#if RPP_HAS_COROUTINES
    using rpp::task;
    using rpp::deferred;
#endif
}

export namespace rpp::detail {
#if RPP_HAS_COROUTINES
    using rpp::detail::task_final_awaiter;
    using rpp::detail::take_result;
    using rpp::detail::task_promise;
    using rpp::detail::task_base;
#endif
}
// GENERATED EXPORTS END

// C++20 module interface unit for <rpp/future.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "future.h"

export module rpp.future;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::cpromise;
    using rpp::async_task;
    using rpp::cfuture;
    using rpp::make_ready_future;
    using rpp::make_exceptional_future;
    using rpp::wait_all;
    using rpp::get_all;
    using rpp::run_tasks;
}
// GENERATED EXPORTS END

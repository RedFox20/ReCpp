// C++20 module interface unit for <rpp/thread_pool.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "thread_pool.h"

export module rpp.thread_pool;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::seconds_t;
    using rpp::fseconds_t;
    using rpp::dseconds_t;
    using rpp::milliseconds_t;
    using rpp::duration_t;
    using rpp::action;
    using rpp::task_delegate;
    using rpp::pool_signal_handler;
    using rpp::pool_trace_provider;
    using rpp::wait_result;
    using rpp::pool_worker;
    using rpp::pool_task_state;
    using rpp::pool_task_handle;
    using rpp::thread_pool;
    using rpp::parallel_for;
    using rpp::parallel_foreach;
    using rpp::parallel_task;
    using rpp::parallel_task_detached;
}
// GENERATED EXPORTS END

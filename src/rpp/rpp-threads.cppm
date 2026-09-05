// C++20 module interface unit for <rpp/threads.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "threads.h"

export module rpp.threads;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;
export import rpp.strview;

export namespace rpp {
    using rpp::set_this_thread_name;
    using rpp::get_this_thread_name;
    using rpp::get_thread_name;
    using rpp::get_thread_id;
    using rpp::get_process_id;
    using rpp::num_physical_cores;
    using rpp::yield;
}
// GENERATED EXPORTS END

// C++20 module interface unit for <rpp/stack_trace.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "stack_trace.h"

export module rpp.stack_trace;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;
export import rpp.strview;

export namespace rpp {
    using rpp::CallstackEntry;
    using rpp::get_address_info;
    using rpp::CALLSTACK_MAX_DEPTH;
    using rpp::get_callstack;
    using rpp::ThreadCallstack;
    using rpp::get_all_callstacks;
    using rpp::format_trace;
    using rpp::stack_trace;
    using rpp::print_trace;
    using rpp::error_with_trace;
    using rpp::traced_exception;
    using rpp::register_segfault_tracer;
}
// GENERATED EXPORTS END

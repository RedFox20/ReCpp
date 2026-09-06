// C++20 module interface unit for <rpp/proc_utils.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "proc_utils.h"

export module rpp.proc_utils;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;

export namespace rpp {
    using rpp::proc_mem_info;
    using rpp::proc_current_mem_used;
    using rpp::cpu_usage_info;
    using rpp::proc_total_cpu_usage;
}
// GENERATED EXPORTS END

// C++20 module interface unit for <rpp/memory_pool.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "memory_pool.h"

export module rpp.memory_pool;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.collections;
export import rpp.config;

export namespace rpp {
    using rpp::pool_types_constructor;
    using rpp::linear_static_pool;
    using rpp::linear_dynamic_pool;
}
// GENERATED EXPORTS END

// C++20 module interface unit for <rpp/load_balancer.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "load_balancer.h"

export module rpp.load_balancer;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;
export import rpp.timepoint;
export import rpp.timer;

export namespace rpp {
    using rpp::load_balancer;
}
// GENERATED EXPORTS END

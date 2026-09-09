// C++20 module interface unit for <rpp/condition_variable.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "condition_variable.h"

export module rpp.condition_variable;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;
export import rpp.debugging;
export import rpp.mutex;
export import rpp.predicates;
export import rpp.timepoint;
export import rpp.timer;

export namespace rpp {
    using rpp::_cv_remaining_duration;
    using rpp::condition_variable;
}
// GENERATED EXPORTS END

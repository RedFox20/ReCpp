// C++20 module interface unit for <rpp/atomic_timepoint.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "atomic_timepoint.h"

export module rpp.atomic_timepoint;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;
export import rpp.timepoint;

export namespace rpp {
    using rpp::AtomicDuration;
    using rpp::AtomicTimePoint;
    using rpp::AtomicTimeSource;
}
// GENERATED EXPORTS END

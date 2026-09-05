// C++20 module interface unit for <rpp/timer.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "timer.h"

export module rpp.timer;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;
export import rpp.timepoint;

export namespace rpp {
    using rpp::Timer;
    using rpp::StopWatch;
    using rpp::ScopedPerfTimer;
}
// GENERATED EXPORTS END

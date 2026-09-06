// C++20 module interface unit for <rpp/math.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "math.h"

export module rpp.math;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.minmax;

export namespace rpp {
    using rpp::PI;
    using rpp::PIf;
    using rpp::SQRT2;
    using rpp::SQRT2f;
    using rpp::radf;
    using rpp::degf;
    using rpp::clamp;
    using rpp::lerp;
    using rpp::lerpInverse;
    using rpp::nearlyZero;
    using rpp::almostEqual;
    using rpp::clampZero;
}
// GENERATED EXPORTS END

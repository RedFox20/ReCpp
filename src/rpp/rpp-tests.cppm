// C++20 module interface unit for <rpp/tests.h>.
// A module cannot export a macro, so an importer adds <rpp/tests.macros.h> for
// TestImpl, TestCase and the Assert family.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "tests.h"

export module rpp.tests;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;
export import rpp.debugging;
export import rpp.math;
export import rpp.minmax;
export import rpp.source_loc;
export import rpp.strview;

export namespace rpp {
    using rpp::test;
    using rpp::test_info;
    using rpp::test_results;
    using rpp::test_factory;
    using rpp::register_test;
    using rpp::TestVerbosity;
    using rpp::Compare;
}
// GENERATED EXPORTS END

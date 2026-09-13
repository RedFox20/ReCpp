// C++20 module interface unit for the rpp.testing headers, owned by tools/gen_module_exports.py.
// The headers stay in the global module fragment, so an importer and an includer share one entity.
module;

#include "tests.h"

export module rpp.testing;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

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

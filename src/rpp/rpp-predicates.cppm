// C++20 module interface unit for <rpp/predicates.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "predicates.h"

export module rpp.predicates;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;

export namespace rpp {
    using rpp::IsCallable;
    using rpp::IsPredicate;
}
// GENERATED EXPORTS END

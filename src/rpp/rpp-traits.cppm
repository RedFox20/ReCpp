// C++20 module interface unit for <rpp/traits.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "traits.h"

export module rpp.traits;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::function_traits;
    using rpp::first_arg_type;
    using rpp::cont_return_t;
    using rpp::task_return_t;
}
// GENERATED EXPORTS END

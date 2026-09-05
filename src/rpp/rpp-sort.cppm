// C++20 module interface unit for <rpp/sort.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "sort.h"

export module rpp.sort;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;

export namespace rpp {
    using rpp::sort_comparison;
    using rpp::contiguous_container;
    using rpp::container_element_t;
    using rpp::insertion_sort;
}
// GENERATED EXPORTS END

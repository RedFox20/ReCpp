// C++20 module interface unit for <rpp/type_traits.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "type_traits.h"

export module rpp.type_traits;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.config;

export namespace rpp {
    using rpp::is_detected;
    using rpp::is_detected_v;
    using rpp::std_to_string_expression;
    using rpp::to_string_expression;
    using rpp::to_string_memb_expression;
    using rpp::get_memb_expression;
    using rpp::set_memb_expression;
    using rpp::has_std_to_string;
    using rpp::has_to_string;
    using rpp::has_to_string_memb;
    using rpp::has_get_memb;
    using rpp::has_set_memb;
    using rpp::has_begin_expression;
    using rpp::has_end_expression;
    using rpp::has_size_expression;
    using rpp::has_c_str_expression;
    using rpp::is_iterable;
    using rpp::is_stringlike;
    using rpp::is_container;
}

export namespace rpp::detail {
    using rpp::detail::void_t;
    using rpp::detail::is_detected;
}
// GENERATED EXPORTS END

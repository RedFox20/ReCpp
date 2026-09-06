// C++20 module interface unit for <rpp/collections.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "collections.h"

export module rpp.collections;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block
export import rpp.sort;

export namespace rpp {
    using rpp::element_range;
    using rpp::range;
    using rpp::element_type_with_const;
    using rpp::slice;
    using rpp::index_range;
    using rpp::operator+;
    using rpp::operator-;
    using rpp::swap;
    using rpp::pop_back;
    using rpp::pop_front;
    using rpp::push_unique;
    using rpp::erase_item;
    using rpp::erase_first_if;
    using rpp::erase_if;
    using rpp::erase_back_swap;
    using rpp::erase_item_back_swap;
    using rpp::erase_back_swap_first_if;
    using rpp::erase_back_swap_all_if;
    using rpp::contains;
    using rpp::append;
    using rpp::find;
    using rpp::find_if;
    using rpp::find_last_if;
    using rpp::find_smallest;
    using rpp::find_largest;
    using rpp::index_of;
    using rpp::any_of;
    using rpp::all_of;
    using rpp::none_of;
    using rpp::sum_all;
    using rpp::transform;
    using rpp::sort;
    using rpp::operator==;
    using rpp::operator!=;
}
// GENERATED EXPORTS END

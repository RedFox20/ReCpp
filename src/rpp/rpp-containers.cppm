// C++20 module interface unit for the rpp.containers headers, owned by tools/gen_module_exports.py.
// The headers stay in the global module fragment, so an importer and an includer share one entity.
module;

#include "collections.h"
#include "memory_pool.h"
#include "load_balancer.h"

export module rpp.containers;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

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
    using rpp::linear_static_pool;
    using rpp::linear_dynamic_pool;
    using rpp::load_balancer;
}
// GENERATED EXPORTS END

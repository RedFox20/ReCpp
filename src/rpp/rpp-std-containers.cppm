// The std container and tuple names a public ReCpp signature writes. `rpp.std` re-exports it.
module;

#include <vector>
#include <array>
#include <deque>
#include <unordered_map>
#include <tuple>
#include <span>
#include <optional>
#include <initializer_list>

export module rpp.std.containers;

export namespace std {
    using std::vector;
    using std::array;
    using std::deque;
    using std::span;
    using std::dynamic_extent;
    using std::unordered_map;
    using std::optional;
    using std::nullopt;
    using std::nullopt_t;
    using std::initializer_list;

    // tuples. std::get stays out, see BUGS.md B21
    using std::tuple;
    using std::tuple_size;
    using std::tuple_element;
    using std::pair;
    using std::make_pair;
}

// An importer includes <new> for the placement operator new a std::vector needs.

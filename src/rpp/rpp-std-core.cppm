// The std exception, trait and utility names a public ReCpp signature writes.
// `rpp.std` re-exports this part.
module;

#include <stdexcept>
#include <exception>
#include <typeinfo>
#include <source_location>
#include <type_traits>
#include <utility>

export module rpp.std.core;

export namespace std {
    // source_loc.h takes one in a public constructor, tests.h takes the other
    using std::source_location;
    using std::type_info;

    // exceptions. std::exception_ptr is absent on purpose, see BUGS.md B19
    using std::exception;
    using std::runtime_error;
    using std::logic_error;
    using std::current_exception;
    using std::rethrow_exception;

    // traits the public signatures name
    using std::decay_t;
    using std::is_same_v;
    using std::is_invocable_v;
    using std::conditional_t;
    using std::enable_if_t;
    using std::move;
    using std::forward;
    using std::swap;
}

// The fixed width integers stay out, because <cstdint> also declares them at global scope.
// A translation unit which imports this and includes an rpp header then reports an ambiguity.

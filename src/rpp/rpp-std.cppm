// Stands in for `import std;`, which gcc-14 does not ship. It carries the std names ReCpp
// puts in a public signature, so a consumer can import instead of including each header.
module;

#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <optional>
#include <stdexcept>
#include <exception>
#include <type_traits>

export module rpp.std;

export namespace std {
    // strings
    using std::string;
    using std::wstring;
    using std::u16string;
    using std::to_string;
    using std::basic_string;
    using std::char_traits;

    // containers
    using std::vector;
    using std::optional;
    using std::nullopt;
    using std::nullopt_t;

    // memory
    using std::unique_ptr;
    using std::shared_ptr;
    using std::weak_ptr;
    using std::make_unique;
    using std::make_shared;
    using std::default_delete;

    // atomics
    using std::atomic;
    using std::atomic_int;
    using std::atomic_bool;
    using std::atomic_int64_t;
    using std::memory_order;
    using std::memory_order_relaxed;
    using std::memory_order_acquire;
    using std::memory_order_release;
    using std::memory_order_seq_cst;

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

// The fixed width integers stay out. <cstdint> also declares them at global scope, so a
// translation unit which imports this and includes any rpp header finds both and reports
// `reference to 'uint32_t' is ambiguous`. They cost almost nothing as a header anyway.

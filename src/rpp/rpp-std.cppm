// Stands in for `import std;`, which gcc-14 does not ship. It carries the std names ReCpp
// puts in a public signature, so a consumer can import instead of including each header.
module;

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <deque>
#include <unordered_map>
#include <tuple>
#include <span>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <optional>
#include <stdexcept>
#include <exception>
#include <typeinfo>
#include <source_location>
#include <type_traits>
#include <utility>
#include <initializer_list>

export module rpp.std;

export namespace std {
    // strings
    using std::string;
    using std::wstring;
    using std::u16string;
    using std::to_string;
    using std::basic_string;
    using std::char_traits;
    using std::string_view;
    using std::wstring_view;

    // containers
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
    using std::memory_order_acq_rel;
    using std::memory_order_seq_cst;

    // locking. rpp::condition_variable::wait_for takes a std::unique_lock and returns a
    // std::cv_status, and close_sync.h names std::shared_mutex in a public alias
    using std::mutex;
    using std::unique_lock;
    using std::lock_guard;
    using std::shared_mutex;
    using std::shared_lock;
    using std::condition_variable;
    using std::cv_status;

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

    // an exported type does not carry its free operators, so `s != "x"` fails in an
    // importer which includes no <string>
    using std::operator==;
    using std::operator!=;
    using std::operator+;
    using std::operator<=>;
}

// An importer includes <new> for the placement operator new a std::vector needs.
// A std::shared_ptr also needs <memory> to link, see BUGS.md B22.

// The fixed width integers stay out, because <cstdint> also declares them at global scope.
// A translation unit which imports this and includes an rpp header then reports an ambiguity.

// <future> stays out of the fragment above, see BUGS.md B20. std::future, std::promise and
// std::future_status go with it, and rpp::cfuture::await_ready() answers without the header.

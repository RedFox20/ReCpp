#pragma once
#include "config.h"
#include <cstdint> // std::int32_t, std::uint32_t
#include <type_traits>

#if !RPP_BARE_METAL
    #include <string> // std::to_string
#endif // !RPP_BARE_METAL

namespace rpp
{
    // config.types.h has no include, so the aliases are checked here. A std::atomic_uint32_t needs an rpp::uint32 to CAS.
    // rpp::int32 stays int, because the int overloads, such as Duration::from_millis(int32), take an int literal.
    static_assert(std::is_same_v<rpp::uint32, std::uint32_t>, "rpp::uint32 must be std::uint32_t");
    static_assert(sizeof(rpp::int32) == 4, "rpp::int32 must be 32 bits");
    static_assert(sizeof(rpp::int16) == 2 && sizeof(rpp::uint16) == 2, "rpp::int16 and rpp::uint16 must be 16 bits");
    static_assert(sizeof(rpp::int64) == 8 && sizeof(rpp::uint64) == 8, "rpp::int64 and rpp::uint64 must be 64 bits");

    // ------------------------------------------------------------------------------------ //

    namespace detect
    {
        template< class... >
        using void_t = void;

        template<typename, template<typename...> class, typename...>
        struct is_detected : std::false_type {};

        template<template<class...> class Operation, typename... Arguments>
        struct is_detected<void_t<Operation<Arguments...>>, Operation, Arguments...> : std::true_type {};
    }

    // the traits below are concepts. This stays for a consumer which brings its own alias
    template<template<class...> class Operation, typename... Arguments>
    using is_detected = detect::is_detected<detect::void_t<>, Operation, Arguments...>;

    template<template<class...> class Operation, typename... Arguments>
    inline constexpr bool is_detected_v = detect::is_detected<detect::void_t<>, Operation, Arguments...>::value;

#if !RPP_BARE_METAL
    /// True when `std::to_string(T)` compiles
    template<class T> concept has_std_to_string = requires { std::to_string(std::declval<T>()); };
#endif

    /// True when an ADL `to_string(T)` compiles
    template<class T> concept has_to_string = requires { to_string(std::declval<T>()); };

    /// True when `T::to_string()` compiles
    template<class T> concept has_to_string_memb = requires { std::declval<T>().to_string(); };

    /// True when `T::get()` compiles
    template<class T> concept has_get_memb = requires { std::declval<T>().get(); };

    /// True when `T::set(U)` compiles
    template<class T, class U> concept has_set_memb = requires { std::declval<T>().set(std::declval<U>()); };

    /// True when `C::contains(T)` compiles
    template<class C, class T> concept has_contains_memb = requires(const C& container, const T& item)
    {
        container.contains(item);
    };

    /// True when T supports range-for
    template<class T> concept is_iterable = requires { std::declval<T>().begin(); std::declval<T>().end(); };

    /// True when `T::c_str()` compiles
    template<class T> concept is_stringlike = requires { std::declval<T>().c_str(); };

    /// True when T iterates and reports a size, and does not look like a string
    template<class T> concept is_container = is_iterable<T> && !is_stringlike<T>
                                          && requires { std::declval<T>().size(); };

    // ------------------------------------------------------------------------------------ //
}

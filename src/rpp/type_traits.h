#pragma once
#include "config.h"
#include <type_traits>

#if !RPP_BARE_METAL
    #include <string> // std::to_string
#endif // !RPP_BARE_METAL

namespace rpp
{
    // ------------------------------------------------------------------------------------ //

    namespace detail
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
    using is_detected = detail::is_detected<detail::void_t<>, Operation, Arguments...>;

    template<template<class...> class Operation, typename... Arguments>
    inline constexpr bool is_detected_v = detail::is_detected<detail::void_t<>, Operation, Arguments...>::value;

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

    /// True when T supports range-for
    template<class T> concept is_iterable = requires { std::declval<T>().begin(); std::declval<T>().end(); };

    /// True when `T::c_str()` compiles
    template<class T> concept is_stringlike = requires { std::declval<T>().c_str(); };

    /// True when T iterates and reports a size, and does not look like a string
    template<class T> concept is_container = is_iterable<T> && !is_stringlike<T>
                                          && requires { std::declval<T>().size(); };

    // ------------------------------------------------------------------------------------ //
}

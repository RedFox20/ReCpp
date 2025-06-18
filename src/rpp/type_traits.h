#pragma once
#include "config.h"
#include <string> // std::to_string
#include <type_traits>

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

    template<template<class...> class Operation, typename... Arguments>
    using is_detected = detail::is_detected<detail::void_t<>, Operation, Arguments...>;

    template<template<class...> class Operation, typename... Arguments>
    constexpr bool is_detected_v = detail::is_detected<detail::void_t<>, Operation, Arguments...>::value;


    template<class T> using std_to_string_expression  = decltype(std::to_string(std::declval<T>()));
    template<class T> using to_string_expression      = decltype(to_string(std::declval<T>()));
    template<class T> using to_string_memb_expression = decltype(std::declval<T>().to_string());
    template<class T> using get_memb_expression       = decltype(std::declval<T>().get());
    template<class T, class U> using set_memb_expression = decltype(std::declval<T>().set(std::declval<U>()));

    template<class T> constexpr bool has_std_to_string  = is_detected_v<std_to_string_expression, T>;
    template<class T> constexpr bool has_to_string      = is_detected_v<to_string_expression, T>;
    template<class T> constexpr bool has_to_string_memb = is_detected_v<to_string_memb_expression, T>;
    template<class T> constexpr bool has_get_memb       = is_detected_v<get_memb_expression, T>;
    template<class T, class U> constexpr bool has_set_memb = is_detected_v<set_memb_expression, T, U>;

    template<class T> using has_begin_expression  = decltype(std::declval<T>().begin());
    template<class T> using has_end_expression    = decltype(std::declval<T>().end());
    template<class T> using has_size_expression   = decltype(std::declval<T>().size());
    template<class T> using has_c_str_expression  = decltype(std::declval<T>().c_str());

    template<class T> constexpr bool is_iterable = is_detected_v<has_begin_expression, T>
                                                && is_detected_v<has_end_expression, T>;

    template<class T> constexpr bool is_stringlike = is_detected_v<has_c_str_expression, T>;

    template<class T> constexpr bool is_container = is_iterable<T>
                                                 && is_detected_v<has_size_expression, T>
                                                 && !is_stringlike<T>;

    // ------------------------------------------------------------------------------------ //
}

#pragma once
/**
 * Useful type traits for function types, lambdas and other standard types
 * Copyright (c) 2017-2018, 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include <type_traits>
// #include <functional> // std::function

namespace rpp
{
    namespace
    {
        template<typename T>
        struct function_traits : function_traits<decltype(&T::operator())> {
        };

        template<typename R, typename... Args>
        struct function_traits<R(Args...)> { // function type
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };
        
        template<typename R, typename... Args>
        struct function_traits<R (*)(Args...)> { // function pointer
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };

        // template<typename R, typename... Args>
        // struct function_traits<std::function<R(Args...)>> {
        //     using ret_type  = R;
        //     using arg_types = std::tuple<Args...>;
        // };

        template<typename T, typename R, typename... Args>
        struct function_traits<R (T::*)(Args...)> { // member func ptr
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };

        template<typename T, typename R, typename... Args>
        struct function_traits<R (T::*)(Args...) const> {  // const member func ptr
            using ret_type  = R;
            using arg_types = std::tuple<Args...>;
        };

        template<typename T>
        using first_arg_type = typename std::tuple_element<0, typename function_traits<T>::arg_types>::type;
    }

    #if RPP_HAS_CXX20

    template<typename Function>
        concept IsFunction = std::is_invocable_v<Function>;

    #endif
}

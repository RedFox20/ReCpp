#pragma once
/**
 * Logging and assertion macros, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 *
 * A C++20 module cannot export a macro, so every macro of <rpp/debugging.h> lives here.
 *
 *   #include <rpp/debugging.h>          // classic: gives the declarations AND these macros
 *   import rpp.debugging;               // module: gives the declarations
 *   #include <rpp/debugging.macros.h>   // module: add this line for the macros
 *
 * Each macro below calls a name that <rpp/debugging.h> or `import rpp.debugging` declares.
 * Include one of the two first. This header declares almost nothing itself.
 *
 * ThrowErr and AssertEx name std::runtime_error, so a user of those adds <stdexcept>.
 * This header does not, because <stdexcept> costs 32k preprocessed lines and config.h costs 46.
 */
#include "config.h" // RPP_ANDROID, RPP_BARE_METAL, RPP_EXTERNC

#ifndef QUIETLOG
#define __log_format(format, file, line, func) "%s:%d %s $ " format, rpp::shorten_filename(file), line, _LogFuncname(func)
#else
#define __log_format(format, file, line, func) "$ " format
#endif

#if __cplusplus

#define _rpp_get_nth_wrap_arg(zero, _12,_11,_10,_9,  _8,_7,_6,_5,  _4,_3,_2,_1,  N_0, ...) N_0
#define _rpp_wrap(x) x

// _rpp_is_empty(__VA_ARGS__) gives 1 for an empty list and 0 otherwise, from 4 probes.
// Probe 2 keeps a leading `(` out of the empty case, see BUGS.md C24.
#define _rpp_arg16(_0,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,...) _15
#define _rpp_has_comma(...) _rpp_wrap(_rpp_arg16(__VA_ARGS__, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,0))
#define _rpp_trigger_paren(...) ,
#define _rpp_paste5(_0,_1,_2,_3,_4) _0##_1##_2##_3##_4
#define _rpp_is_empty_case_0001 ,
#define _rpp_is_empty_impl(_0,_1,_2,_3) _rpp_has_comma(_rpp_paste5(_rpp_is_empty_case_,_0,_1,_2,_3))
#define _rpp_is_empty(...) _rpp_wrap(_rpp_is_empty_impl(_rpp_has_comma(__VA_ARGS__), \
    _rpp_has_comma(_rpp_trigger_paren __VA_ARGS__), _rpp_has_comma(__VA_ARGS__ (/*empty*/)), \
    _rpp_has_comma(_rpp_trigger_paren __VA_ARGS__ (/*empty*/))))
#define _rpp_comma_if_0 ,
#define _rpp_comma_if_1
#define _rpp_cat2(a,b) a##b
#define _rpp_cat(a,b) _rpp_cat2(a,b)

// _va_comma(__VA_ARGS__) and its fallback give `,` for a non-empty list, nothing for an empty one.
// The fallback expands the list first, so an argument which expands to nothing also reads as empty.
#define _rpp_va_comma_fallback(...) _rpp_wrap(_rpp_cat(_rpp_comma_if_, _rpp_is_empty(__VA_ARGS__)))

// MSVC needs /Zc:preprocessor for __VA_OPT__, and _MSVC_TRADITIONAL==0 confirms it is active.
#if (!defined(_MSC_VER) && __cplusplus >= 202002L) \
 || (defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL == 0)
#define _va_comma(...) __VA_OPT__(,)
#else
#define _va_comma(...) _rpp_va_comma_fallback(__VA_ARGS__)
#endif

#define _wa0(...)
#define _wa1(z, x)       , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x)
#define _wa2(z, x, ...)  , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa1(z, __VA_ARGS__))
#define _wa3(z, x, ...)  , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa2(z, __VA_ARGS__))
#define _wa4(z, x, ...)  , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa3(z, __VA_ARGS__))
#define _wa5(z, x, ...)  , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa4(z, __VA_ARGS__))
#define _wa6(z, x, ...)  , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa5(z, __VA_ARGS__))
#define _wa7(z, x, ...)  , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa6(z, __VA_ARGS__))
#define _wa8(z, x, ...)  , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa7(z, __VA_ARGS__))
#define _wa9(z, x, ...)  , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa8(z, __VA_ARGS__))
#define _wa10(z, x, ...) , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa9(z, __VA_ARGS__))
#define _wa11(z, x, ...) , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa10(z, __VA_ARGS__))
#define _wa12(z, x, ...) , rpp::__wrap<rpp::__clean_type<decltype(x)>>::w(x) _rpp_wrap(_wa11(z, __VA_ARGS__))

#define _rpp_wrap_args_2(...) _rpp_wrap( _rpp_get_nth_wrap_arg(__VA_ARGS__,  _wa12,_wa11,_wa10,_wa9, \
                                                       _wa8,_wa7,_wa6,_wa5,  _wa4,_wa3,_wa2,_wa1,  _wa0)(__VA_ARGS__) )
#define _rpp_wrap_args(...) _rpp_wrap( _rpp_wrap_args_2(0 _va_comma(__VA_ARGS__) __VA_ARGS__) )
// the tests reach the fallback path through this, which __VA_OPT__ hides on a conforming preprocessor
#define _rpp_wrap_args_fallback(...) _rpp_wrap( _rpp_wrap_args_2(0 _rpp_va_comma_fallback(__VA_ARGS__) __VA_ARGS__) )

#else // C:
  #define _rpp_wrap_args(...) , ##__VA_ARGS__
#endif


// wraps and formats message string for assertions, std::string and rpp::strview are wrapped and .c_str() called
#define _rpp_assert_format(fmt, ...) _FmtString(fmt _rpp_wrap_args(__VA_ARGS__) )


#if defined __APPLE__ || defined __clang__ // iOS or just clang
#  if RPP_ANDROID
#    define __assertion_failure(fmt,...) \
    __assert2(rpp::shorten_filename(__FILE__), __LINE__, _LogFuncname(__FUNCTION__), _rpp_assert_format(fmt, ##__VA_ARGS__))
#  elif __APPLE__
RPP_EXTERNC void __assert_rtn(const char *, const char *, int, const char *) __dead2 __disable_tail_calls;
#    define __assertion_failure(fmt,...) \
    __assert_rtn(_LogFuncname(__FUNCTION__), rpp::shorten_filename(__FILE__), __LINE__, _rpp_assert_format(fmt, ##__VA_ARGS__))
#  else
#    define  __assertion_failure(fmt,...) do { \
        RppAssertFail(_rpp_assert_format(fmt, ##__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__); } while (0)
#  endif
#elif _MSC_VER // Windows VC++
#  ifndef _DEBUG
#    define __assertion_failure(fmt,...) do { \
        __debugbreak(); RppAssertFail(_rpp_assert_format(fmt, ##__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__); } while (0)
#  else // MSVC++ debug assert is quite unique since it supports Format strings. Wish all toolchains did that:
#    define __assertion_failure(fmt,...) do { \
    _CrtDbgReport(_CRT_ASSERT, rpp::shorten_filename(__FILE__), __LINE__, "libReCpp", fmt _rpp_wrap_args(__VA_ARGS__) ); } while (0)
#  endif
#elif defined __GNUC__ // other clang, mingw or linux gcc
#  define  __assertion_failure(fmt,...) do { \
    RppAssertFail(_rpp_assert_format(fmt, ##__VA_ARGS__), __FILE__, __LINE__, __FUNCTION__); } while (0)
#else
#  error Debugging Assert not defined for this compiler toolkit!
#endif

#if defined(DEBUG) || defined(_DEBUG) || defined(BETA) || defined(RPP_DEBUG)
#  define _rpp_debug_assert __assertion_failure
#else
#  define _rpp_debug_assert(...) /*nothing in release builds*/
#endif


/**
 * Logs an info message to the backing error mechanism
 * No assertions are triggered.
 * NO FILE:LINE information is given. Info logs don't need it.
 */
#define LogInfo(format, ...) _LogInfo("$ " format _rpp_wrap_args(__VA_ARGS__) )

/**
 * Logs a warning to the backing error mechanism
 * No assertions are triggered.
 */
#define LogWarning(format, ...) _LogWarning(__log_format(format, __FILE__, __LINE__, __FUNCTION__) _rpp_wrap_args(__VA_ARGS__) )

/**
 * Logs an error to the backing error mechanism
 * An ASSERT is triggered during DEBUG runs.
 */
#define LogError(format, ...) do { \
    _LogError(__log_format(format, __FILE__, __LINE__, __FUNCTION__) _rpp_wrap_args(__VA_ARGS__) ); \
    _rpp_debug_assert(format, ##__VA_ARGS__); \
} while(0)

// Logs an info message with custom file, line, func sources
#define LogInfoFL(file, line, func, format, ...) _LogInfo(__log_format(format, file, line, func) _rpp_wrap_args(__VA_ARGS__) )
// Logs a warning with custom file, line, func sources
#define LogWarningFL(file, line, func, format, ...) _LogWarning(__log_format(format, file, line, func) _rpp_wrap_args(__VA_ARGS__) )
// Logs an error with custom file, line, func sources
#define LogErrorFL(file, line, func, format, ...) _LogError(__log_format(format, file, line, func) _rpp_wrap_args(__VA_ARGS__) )

// LogError for a condition with message in all types of builds
// @warning This is not a fatal assert!
#define Assert(expression, format, ...) do { if (!(expression)) LogError(format, ##__VA_ARGS__ ); } while(0)


// LogError for a condition with no message formatting
// @warning This is not a fatal assert!
#define AssertExpr(expression) do { if (!(expression)) LogError("Assert failed: %s", #expression); } while(0)

#if defined(DEBUG) || defined(_DEBUG) || defined(BETA)
// Asserts for a condition with message only in DEBUG builds
#  define DbgAssert(expression, format, ...) do { if (!(expression)) LogError(format, ##__VA_ARGS__ ); } while(0)
#else
#  define DbgAssert(expression, format, ...) /*do nothing in release builds*/
#endif

#ifdef __cplusplus

// logs an std::exception; this is piped into the special exception handler @see SetLogExceptHandler()
// triggers an assertion in debug builds
#define LogExcept(std_except, format, ...) do { \
    _LogExcept(std_except.what(), __log_format(format, __FILE__, __LINE__, __FUNCTION__) _rpp_wrap_args(__VA_ARGS__) ); \
    _rpp_debug_assert(format ": %s", ##__VA_ARGS__, std_except.what() ); \
} while(0)

// uses printf style formatting to build an exception message
#define ThrowErrType(exceptionClass, format, ...) do { \
    auto* __formatted_error__ = _FmtString(format _rpp_wrap_args(__VA_ARGS__) ); \
    throw exceptionClass(__formatted_error__); \
} while(0)

// logs error message, triggers an assertion and throws an std::runtime_error
#define ThrowErr(format, ...) ThrowErrType(std::runtime_error, format, ##__VA_ARGS__)

// Asserts an expression and throws if the expression fails
// A custom message must be provided
#define AssertEx(expression, format, ...) do { \
    if (!(expression)) { \
        auto* __formatted_error__ = _FmtString(format _rpp_wrap_args(__VA_ARGS__) ); \
        throw std::runtime_error(__formatted_error__); \
    } \
} while(0)

#endif // __cplusplus

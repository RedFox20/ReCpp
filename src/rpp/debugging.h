#pragma once
/**
 * Cross platform debugging interface, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <assert.h> // platform specific assert stuff
#include <stdarg.h>
#include <stdio.h> // fprintf
#include "config.h"

#ifdef _MSC_VER
#  include <crtdbg.h>
#  define PRINTF_CHECKFMT
#  define PRINTF_CHECKFMT2
#  define PRINTF_FMTSTR _In_z_ _Printf_format_string_
#else
#  define PRINTF_CHECKFMT __attribute__((__format__ (__printf__, 1, 2)))
#  define PRINTF_CHECKFMT2 __attribute__((__format__ (__printf__, 2, 3)))
#  define PRINTF_FMTSTR
#endif
#if __GNUG__
#  pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif

typedef enum {
    LogSeverityInfo,  // merely information;
    LogSeverityWarn,  // warning unexpected behaviour; -- but we can recover
    LogSeverityError, // critical error or bug; -- a spectacular failure
} LogSeverity;

typedef void (*LogMessageCallback) (LogSeverity severity, const char* message, int len);
typedef void (*LogEventCallback) (const char* eventName, const char* message, int len);
typedef void (*LogExceptCallback)(const char* message, const char* exception);

/** Sets the callback handler for any error messages */
RPPCAPI void SetLogHandler(LogMessageCallback loghandler);

// legacy ALIAS for SetLogHandler
RPPCAPI
#if __cplusplus
[[deprecated("Use SetLogHandler() instead")]]
#endif
void SetLogErrorHandler(LogMessageCallback loghandler);

/** Sets the callback handler for event messages  */
RPPCAPI void SetLogEventHandler(LogEventCallback eventHandler);

/** Sets the callback handler for C++ messages. You can even intercept these in C. */
RPPCAPI void SetLogExceptHandler(LogExceptCallback exceptHandler);

/** This will remove function and lambda name information from the logs */
RPPCAPI void LogDisableFunctionNames();

/** Sets the default log severity filter: if (severity >= filter) log(..)
 *  This defaults to LogSeverityInfo, which is the most verbose
 *  If Debugging.c is compiled with QUIETLOG, then it defaults to LogSeverityWarn
 **/
RPPCAPI void SetLogSeverityFilter(LogSeverity filter);
RPPCAPI LogSeverity GetLogSeverityFilter();

/**
 * Writes a message to default output. On most platforms this is stdout/stderr
 * On Android this writes to Android log, using the given tag.
 * @note NEWLINE is appended automatically
 * @param tag Android log tag. Not used on other platforms.
 * @param severity Info/Warning/Error
 * @param err String
 * @param len Length of string err
 */
RPPCAPI void LogWriteToDefaultOutput(const char* tag, LogSeverity severity, const char* err, int len);
RPPCAPI void LogEventToDefaultOutput(const char* tag, const char* eventName, const char* message, int len);

/** Logs an error to the backing error mechanism */
RPPCAPI void LogFormatv(LogSeverity severity, const char* format, va_list ap);
RPPCAPI void LogWrite(LogSeverity severity, const char* message, int len);
RPPCAPI void _LogInfo    (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
RPPCAPI void _LogWarning (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
RPPCAPI void _LogError   (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
RPPCAPI void  LogEvent   (const char* eventName,     PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT2;
RPPCAPI void _LogExcept  (const char* exceptionWhat, PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT2;
RPPCAPI const char* _FmtString  (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
RPPCAPI const char* _LogFilename(const char* longFilePath); // gets a shortened filepath substring
RPPCAPI const char* _LogFuncname(const char* longFuncName); // shortens the func name

#ifndef QUIETLOG
#define __log_format(format, file, line, func) "%s:%d %s $ " format, _LogFilename(file), line, _LogFuncname(func)
#else
#define __log_format(format, file, line, func) "$ " format
#endif

#if __cplusplus
#include <string>
#if __APPLE__ && __OBJC__
#import <Foundation/NSString.h>
#endif

namespace rpp
{
    template<class T>
    inline const T&    __wrap_arg(const T& arg)            { return arg; }
    inline const char* __wrap_arg(const std::string& arg)  { return arg.c_str();   }

    #if __APPLE__ && __OBJC__
    inline const char* __wrap_arg(NSString* arg) { return arg.UTF8String; }
    #endif
}

#define _rpp_get_nth_wrap_arg(zero, _12,_11,_10,_9,  _8,_7,_6,_5,  _4,_3,_2,_1,  N_0, ...) N_0
#define _rpp_wrap(x) x

#define _rpp_z
#define _rpp_c ,
#define _spaces_on_empty_token(...) ,,,, ,,,, ,,,,
#define _get_nth_comma(_12,_11,_10,_9,  _8,_7,_6,_5,  _4,_3,_2,_1,  N_0, ...) N_0
#define _va_comma2(...) _rpp_wrap(_get_nth_comma(__VA_ARGS__,  _rpp_c,_rpp_c,_rpp_c,_rpp_c, \
                                  _rpp_c,_rpp_c,_rpp_c,_rpp_c, _rpp_c,_rpp_c,_rpp_c,_rpp_c, _rpp_z) )
#define _va_comma(...) _va_comma2(_spaces_on_empty_token __VA_ARGS__ (/*empty*/))

#define _wa0(...)
#define _wa1(z, x)       , rpp::__wrap_arg(x)
#define _wa2(z, x, ...)  , rpp::__wrap_arg(x) _rpp_wrap(_wa1(z, __VA_ARGS__))
#define _wa3(z, x, ...)  , rpp::__wrap_arg(x) _rpp_wrap(_wa2(z, __VA_ARGS__))
#define _wa4(z, x, ...)  , rpp::__wrap_arg(x) _rpp_wrap(_wa3(z, __VA_ARGS__))
#define _wa5(z, x, ...)  , rpp::__wrap_arg(x) _rpp_wrap(_wa4(z, __VA_ARGS__))
#define _wa6(z, x, ...)  , rpp::__wrap_arg(x) _rpp_wrap(_wa5(z, __VA_ARGS__))
#define _wa7(z, x, ...)  , rpp::__wrap_arg(x) _rpp_wrap(_wa6(z, __VA_ARGS__))
#define _wa8(z, x, ...)  , rpp::__wrap_arg(x) _rpp_wrap(_wa7(z, __VA_ARGS__))
#define _wa9(z, x, ...)  , rpp::__wrap_arg(x) _rpp_wrap(_wa8(z, __VA_ARGS__))
#define _wa10(z, x, ...) , rpp::__wrap_arg(x) _rpp_wrap(_wa9(z, __VA_ARGS__))
#define _wa11(z, x, ...) , rpp::__wrap_arg(x) _rpp_wrap(_wa10(z, __VA_ARGS__))
#define _wa12(z, x, ...) , rpp::__wrap_arg(x) _rpp_wrap(_wa11(z, __VA_ARGS__))

#define _rpp_wrap_args_2(...) _rpp_wrap( _rpp_get_nth_wrap_arg(__VA_ARGS__,  _wa12,_wa11,_wa10,_wa9, \
                                                       _wa8,_wa7,_wa6,_wa5,  _wa4,_wa3,_wa2,_wa1,  _wa0)(__VA_ARGS__) )
#define _rpp_wrap_args(...) _rpp_wrap( _rpp_wrap_args_2(0 _va_comma(__VA_ARGS__) __VA_ARGS__) )

#else // C:
  #define _rpp_wrap_args(...) , ##__VA_ARGS__
#endif


// wraps and formats message string for assertions, std::string and rpp::strview are wrapped and .c_str() called
#define _rpp_assert_format(fmt, ...) _FmtString(fmt _rpp_wrap_args(__VA_ARGS__) )


#if defined __APPLE__ || defined __clang__ // iOS or just clang
#  if __ANDROID__
#    define __assertion_failure(fmt,...) \
    __assert2(_LogFilename(__FILE__), __LINE__, _LogFuncname(__FUNCTION__), _rpp_assert_format(fmt, ##__VA_ARGS__))
#  elif __APPLE__
RPP_EXTERNC void __assert_rtn(const char *, const char *, int, const char *) __dead2 __disable_tail_calls;
#    define __assertion_failure(fmt,...) \
    __assert_rtn(_LogFuncname(__FUNCTION__), _LogFilename(__FILE__), __LINE__, _rpp_assert_format(fmt, ##__VA_ARGS__))
#  else
#    define  __assertion_failure(fmt,...) assert(false)
#  endif
#elif _MSC_VER // Windows VC++
#  ifndef _DEBUG
#    define __assertion_failure(fmt,...) do { __debugbreak(); } while (0)
#  else // MSVC++ debug assert is quite unique since it supports Format strings. Wish all toolchains did that:
#    define __assertion_failure(fmt,...) do { \
    _CrtDbgReport(_CRT_ASSERT, _LogFilename(__FILE__), __LINE__, "libReCpp", fmt _rpp_wrap_args(__VA_ARGS__) ); } while (0)
#  endif
#elif defined __GNUC__ // other clang, mingw or linux gcc
#  define  __assertion_failure(fmt,...) assert(false)
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
#include <stdexcept>

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

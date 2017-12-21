#pragma once
/**
 * Cross platform debugging interface, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <assert.h> // platform specific assert stuff
#include <stdarg.h>
#include <stdio.h> // fprintf

#ifdef _MSC_VER
#  define PRINTF_CHECKFMT
#  define PRINTF_CHECKFMT2
#  define PRINTF_FMTSTR _In_z_ _Printf_format_string_
#else
#  define PRINTF_CHECKFMT __attribute__((__format__ (__printf__, 1, 2)))
#  define PRINTF_CHECKFMT2 __attribute__((__format__ (__printf__, 2, 3)))
#  define PRINTF_FMTSTR
#endif

#ifdef __cplusplus
#  define EXTERNC extern "C"
#else
#  define EXTERNC
#endif

#ifndef RPPAPI
#  if _MSC_VER
#    define RPPAPI __declspec(dllexport)
#  else // clang/gcc
#    define RPPAPI __attribute__((visibility("default")))
#  endif
#endif

#ifndef RPPCAPI
#  define RPPCAPI EXTERNC RPPAPI
#endif

typedef enum {
    LogSeverityInfo,  // merely information;
    LogSeverityWarn,  // warning unexpected behaviour; -- but we can recover
    LogSeverityError, // critical error or bug; -- a spectacular failure
} LogSeverity;

/** Error message callback */
typedef void (*LogErrorCallback)(LogSeverity severity, const char* err, int len);
typedef void (*LogEventCallback)(const char* eventName, const char* message, int len);
typedef void (*LogExceptCallback)(const char* message, const char* exception);


/** Sets the callback handler for any error messages */
RPPCAPI void SetLogErrorHandler(LogErrorCallback errfunc);

/** Sets the callback handler for event messages  */
RPPCAPI void SetLogEventHandler(LogEventCallback eventFunc);

/** Sets the callback handler for C++ messages. You can even intercept these in C. */
RPPCAPI void SetLogExceptHandler(LogExceptCallback exceptFunc);

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
 * @param tag Android log tag. Not used on other platforms.
 */
RPPCAPI void LogWriteToDefaultOutput(const char* tag, LogSeverity severity, const char* err, int len);
RPPCAPI void LogEventToDefaultOutput(const char* tag, const char* eventName, const char* message, int len);

/** Logs an error to the backing error mechanism (Crashlytics on iOS) */
RPPCAPI void LogFormatv(LogSeverity severity, const char* format, va_list ap);
RPPCAPI void _LogInfo    (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
RPPCAPI void _LogWarning (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
RPPCAPI void _LogError   (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
RPPCAPI void LogEvent    (const char* eventName,     PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT2;
RPPCAPI void _LogExcept  (const char* exceptionWhat, PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT2;
RPPCAPI const char* _FmtString  (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
RPPCAPI const char* _LogFilename(const char* longFilePath); // gets a shortened filepath substring
RPPCAPI const char* _LogFuncname(const char* longFuncName); // shortens the func name

/** Provide special string overloads for C++ */
#if __cplusplus && __has_include("sprint.h")
#include "sprint.h"
void Log(LogSeverity severity, rpp::strview message);
template<class T, class... Args> void Log(LogSeverity severity, const T& first, const Args&... args)
{
    if (severity < GetLogSeverityFilter())
        return;
    rpp::string_buffer sb;
    sb.prettyprint(first, args...);
    Log(severity, sb.view());
}
template<class T, class... Args> inline void LogInf(const T& first, const Args&... args) {
    Log(LogSeverityInfo, first, args...);
}
template<class T, class... Args> inline void LogWarn(const T& first, const Args&... args) {
    Log(LogSeverityWarn, first, args...);

}
template<class T, class... Args> inline void LogErr(const T& first, const Args&... args) {
    Log(LogSeverityError, first, args...);
}
#endif


#ifndef QUIETLOG
#define __log_format(format, file, line, func) ("%s:%d %s $ " format), _LogFilename(file), line, _LogFuncname(func)
#else
#define __log_format(format, file, line, func) ("$ " format)
#endif

#if defined(DEBUG) || defined(_DEBUG) || defined(BETA) || defined(RPP_DEBUG)
#  if defined __APPLE__ || defined __clang__ // iOS or just clang
#    if __ANDROID__
#      define __assertion_failure(msg) __assert2(_LogFilename(__FILE__), __LINE__, _LogFuncname(__FUNCTION__), msg)
#    elif __APPLE__
#      define __assertion_failure(msg) __assert_rtn(_LogFuncname(__FUNCTION__), _LogFilename(__FILE__), __LINE__, msg)
#    else
#      define __assertion_failure(msg) __assert_fail(msg, _LogFilename(__FILE__), __LINE__, _LogFuncname(__FUNCTION__))
#    endif
#  elif _MSC_VER // Windows VC++
#    define __assertion_failure(msg) do { __debugbreak(); } while (0)
#  elif defined __GNUC__ // other clang, mingw or linux gcc
#    define  __assertion_failure(msg) __assert_fail(msg, _LogFilename(__FILE__), __LINE__, _LogFuncname(__FUNCTION__))
#  else
#    error Debugging Assert not defined for this compiler toolkit!
#  endif
#else
#  define __assertion_failure(msg) /*nothing in release builds*/
#endif

#if __cplusplus
template<class T>
inline const T&    __wrap_arg(const T& arg)            { return arg; }
inline const char* __wrap_arg(const std::string& arg)  { return arg.c_str();   }
inline const char* __wrap_arg(const rpp::strview& arg) { return arg.to_cstr(); }
inline int __wrap_arg() { return 0; } // default expansion case if no varargs

#define __get_nth_arg(_unused, _8, _7, _6, _5, _4, _3, _2, _1, N_0, ...) N_0
#define __wrap_args0(...)

// MSVC needs a proxy macro to properly expand __VA_ARGS__
#if _MSC_VER
#define __wrap_exp(x) x
#define __wrap_args1(...)    , __wrap_arg(__VA_ARGS__)
#define __wrap_args2(x, ...) , __wrap_arg(x) __wrap_exp(__wrap_args1(__VA_ARGS__))
#define __wrap_args3(x, ...) , __wrap_arg(x) __wrap_exp(__wrap_args2(__VA_ARGS__))
#define __wrap_args4(x, ...) , __wrap_arg(x) __wrap_exp(__wrap_args3(__VA_ARGS__))
#define __wrap_args5(x, ...) , __wrap_arg(x) __wrap_exp(__wrap_args4(__VA_ARGS__))
#define __wrap_args6(x, ...) , __wrap_arg(x) __wrap_exp(__wrap_args5(__VA_ARGS__))
#define __wrap_args7(x, ...) , __wrap_arg(x) __wrap_exp(__wrap_args6(__VA_ARGS__))
#define __wrap_args8(x, ...) , __wrap_arg(x) __wrap_exp(__wrap_args7(__VA_ARGS__))

#define __wrap_args(...) __wrap_exp(__get_nth_arg(0, ##__VA_ARGS__, \
                                    __wrap_args8, __wrap_args7, __wrap_args6, __wrap_args5, \
                                    __wrap_args4, __wrap_args3, __wrap_args2, __wrap_args1, __wrap_args0)(##__VA_ARGS__))
#else
#define __wrap_args1(x)      , __wrap_arg(x)
#define __wrap_args2(x, ...) , __wrap_arg(x) __wrap_args1(__VA_ARGS__)
#define __wrap_args3(x, ...) , __wrap_arg(x) __wrap_args2(__VA_ARGS__)
#define __wrap_args4(x, ...) , __wrap_arg(x) __wrap_args3(__VA_ARGS__)
#define __wrap_args5(x, ...) , __wrap_arg(x) __wrap_args4(__VA_ARGS__)
#define __wrap_args6(x, ...) , __wrap_arg(x) __wrap_args5(__VA_ARGS__)
#define __wrap_args7(x, ...) , __wrap_arg(x) __wrap_args6(__VA_ARGS__)
#define __wrap_args8(x, ...) , __wrap_arg(x) __wrap_args7(__VA_ARGS__)

#define __wrap_args(...) __get_nth_arg(0, ##__VA_ARGS__, \
                            __wrap_args8, __wrap_args7, __wrap_args6, __wrap_args5, \
                            __wrap_args4, __wrap_args3, __wrap_args2, __wrap_args1, __wrap_args0)(__VA_ARGS__)
#endif

#else // C:
  #define __wrap_args(...) , ##__VA_ARGS__
#endif

/**
 * Logs an info message to the backing error mechanism (Crashlytics on iOS)
 * No assertions are triggered.
 * NO FILE:LINE information is given. Info logs don't need it.
 */
#define LogInfo(format, ...) _LogInfo(("$ " format) __wrap_args(__VA_ARGS__) )
/**
 * Logs a warning to the backing error mechanism (Crashlytics on iOS)
 * No assertions are triggered.
 */
#define LogWarning(format, ...) _LogWarning(__log_format(format, __FILE__, __LINE__, __FUNCTION__) __wrap_args(__VA_ARGS__) )
/**
 * Logs an error to the backing error mechanism (Crashlytics on iOS)
 * An ASSERT is triggered during DEBUG runs.
 */
#define LogError(format, ...) do { \
    _LogError(__log_format(format, __FILE__, __LINE__, __FUNCTION__) __wrap_args(__VA_ARGS__) ); \
    __assertion_failure(_FmtString(format __wrap_args(__VA_ARGS__) )); \
} while(0)

// Logs an info message with custom file, line, func sources
#define LogInfoFL(file, line, func, format, ...) _LogInfo(__log_format(format, file, line, func) __wrap_args(__VA_ARGS__) )
// Logs a warning with custom file, line, func sources
#define LogWarningFL(file, line, func, format, ...) _LogWarning(__log_format(format, file, line, func) __wrap_args(__VA_ARGS__) )
// Logs an error with custom file, line, func sources
#define LogErrorFL(file, line, func, format, ...) _LogError(__log_format(format, file, line, func) __wrap_args(__VA_ARGS__) )

// Asserts for a condition with message in all types of builds
#define Assert(expression, format, ...) do { if (!(expression)) LogError(format, ##__VA_ARGS__ ); } while(0)

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
    _LogExcept(std_except.what(), __log_format(format, __FILE__, __LINE__, __FUNCTION__) __wrap_args(__VA_ARGS__) ); \
    __assertion_failure(_FmtString((format ": %s") __wrap_args(__VA_ARGS__), std_except.what() )); \
} while(0)

// logs error message, triggers an assertion and throws exceptionType
#define ThrowErrType(exceptionClass, format, ...) do { \
    _LogError(__log_format(format, __FILE__, __LINE__, __FUNCTION__) __wrap_args(__VA_ARGS__) ); \
    auto* __formatted_error__ = _FmtString(format __wrap_args(__VA_ARGS__) ); \
    __assertion_failure(__formatted_error__); \
    throw exceptionClass(__formatted_error__); \
} while(0)

// logs error message, triggers an assertion and throws an std::runtime_error
#define ThrowErr(format, ...) ThrowErrType(std::runtime_error, format, ##__VA_ARGS__)

#endif // __cplusplus

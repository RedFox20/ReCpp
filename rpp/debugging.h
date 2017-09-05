#ifndef RPP_DEBUGGING_H
#define RPP_DEBUGGING_H
/**
 * Cross platform debugging interface, Copyright (c) 2017 - Jorma Rebane
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

typedef enum {
    LogSeverityInfo,  // merely information;
    LogSeverityWarn,  // warning unexpected behaviour; -- but we can recover
    LogSeverityError, // critical error or bug; -- a spectacular failure
} LogSeverity;

/** Error message callback */
typedef void (*LogErrorCallback)(LogSeverity severity, const char* err, int len);
typedef void (*LogEventCallback)(const char* eventName, const char* message, int len);


/** Sets the callback handler for any error messages */
EXTERNC void SetLogErrorHandler(LogErrorCallback errfunc);

/** Sets the callback handler for event messages  */
EXTERNC void SetLogEventHandler(LogEventCallback eventFunc);

/** This will remove function and lambda name information from the logs */
EXTERNC void LogDisableFunctionNames();

/** Sets the default log severity filter: if (severity >= filter) log(..)
 *  This defaults to LogSeverityInfo, which is the most verbose
 *  If Debugging.c is compiled with QUIETLOG, then it defaults to LogSeverityWarn
 **/
EXTERNC void SetLogSeverityFilter(LogSeverity filter);
EXTERNC LogSeverity GetLogSeverityFilter();

/** Logs an error to the backing error mechanism (Crashlytics on iOS) */
EXTERNC void LogFormatv(LogSeverity severity, const char* format, va_list ap);
EXTERNC void _LogInfo    (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
EXTERNC void _LogWarning (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
EXTERNC void _LogError   (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
EXTERNC void LogEvent    (const char* eventName, PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT2;
EXTERNC const char* _FmtString  (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT;
EXTERNC const char* _LogFilename(const char* longFilePath); // gets a shortened filepath substring
EXTERNC const char* _LogFuncname(const char* longFuncName); // shortens the func name


/** Provide special string overloads for C++ */
#ifdef __cplusplus
#include "strview.h"
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
#define __log_format(format, file, line, func) ("%s:%d %s $ " format), file, line, _LogFuncname(func)
#else
#define __log_format(format, file, line, func) ("$ " format)
#endif

#if defined(DEBUG) || defined(_DEBUG) || defined(BETA)
#  ifdef __APPLE__ // clang iOS
#    define  __assertion_failure(msg) __assert_rtn(_LogFuncname(__FUNCTION__), _LogFilename(__FILE__), __LINE__, msg)
#  elif defined __GNUC__ // other clang, mingw or linux gcc
#    define  __assertion_failure(msg) __assert_fail(msg, _LogFilename(__FILE__), __LINE__, _LogFuncname(__FUNCTION__))
#  elif _MSC_VER // Windows VC++
#    define __assertion_failure(msg) do { __debugbreak(); } while (0)
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

#if defined(DEBUG) || defined(_DEBUG) || defined(BETA)
// Asserts for a condition with message
#  define Assert(expression, format, ...) do { if (!(expression)) LogError(format, ##__VA_ARGS__ ); } while(0)
#else
#  define Assert(expression, format, ...) /*do nothing in release builds*/
#endif

#ifdef __cplusplus
#include <stdexcept>

// logs error message, triggers an assertion and throws an std::runtime_error
#define ThrowErr(format, ...) do { \
    _LogError(__log_format(format, __FILE__, __LINE__, __FUNCTION__) __wrap_args(__VA_ARGS__) ); \
	auto* __formatted_error__ = _FmtString(format __wrap_args(__VA_ARGS__) ); \
    __assertion_failure(__formatted_error__); \
	throw std::runtime_error(__formatted_error__); \
} while(0)

#endif

#endif /* RPP_DEBUGGING_H */

#pragma once
/**
 * Cross platform debugging interface, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include "log_colors.h"
#include <assert.h> // platform specific assert stuff
#include <stdarg.h>
#include <stdio.h> // fprintf

// TODO: currently we ignore this bug, but this needs to be fixed in unwrap macros
#if __GNUG__
#  pragma GCC diagnostic ignored "-Wformat-extra-args"
#  ifdef NDEBUG
#  else
#    include <assert.h> // __assert_fail
#  endif
#endif

typedef enum {
    LogSeverityInfo,  // merely information;
    LogSeverityWarn,  // warning unexpected behaviour; -- but we can recover
    LogSeverityError, // critical error or bug; -- a spectacular failure
} LogSeverity;

typedef void (*LogMessageCallback) (LogSeverity severity, const char* message, int len);
typedef void (*LogExceptCallback)(const char* message, const char* exception);

/** Sets the callback handler for any error messages */
RPPCAPI void SetLogHandler(LogMessageCallback loghandler) noexcept;

/** Sets the callback handler for C++ messages. You can even intercept these in C. */
RPPCAPI void SetLogExceptHandler(LogExceptCallback exceptHandler) noexcept;

/** This will remove function and lambda name information from the logs */
RPPCAPI void LogDisableFunctionNames() noexcept;

/** Sets the default log severity filter: if (severity >= filter) log(..)
 *  This defaults to LogSeverityInfo, which is the most verbose
 *  If Debugging.c is compiled with QUIETLOG, then it defaults to LogSeverityWarn
 **/
RPPCAPI void SetLogSeverityFilter(LogSeverity filter) noexcept;
RPPCAPI LogSeverity GetLogSeverityFilter() noexcept;

/**
 * Prefixes all log entries with a time-of-day timestamp in the format of:
 * hh:mm:ss.MMMms, e.g. 21:24:13.172ms
 * @param enable True to enable timestamps, false to disable
 * @param precision[3] The number of decimal places to show: 3 for milliseconds, 6 for micros.
 */
RPPCAPI void LogEnableTimestamps(bool enable, int precision = 3) noexcept;

/**
 * Adds a time offset to the log timestamps. This is useful for syncing logs.
 * The offset is in nanoseconds.
 */
RPPCAPI void LogSetTimeOffset(rpp::int64 offset_nanos) noexcept;

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

// Provides a generic assertion fail, which calls std::terminate
RPPCAPI RPP_NORETURN void RppAssertFail(const char* message, const char* file,
                                        unsigned int line, const char* function);

/** Logs an error to the backing error mechanism */
RPPCAPI void LogFormatv(LogSeverity severity, const char* format, va_list ap);
RPPCAPI void LogWrite(LogSeverity severity, const char* message, int len);
RPPCAPI void _LogInfo    (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT1;
RPPCAPI void _LogWarning (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT1;
RPPCAPI void _LogError   (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT1;
RPPCAPI void _LogExcept  (const char* exceptionWhat, PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT2;
RPPCAPI const char* _FmtString  (PRINTF_FMTSTR const char* format, ...) PRINTF_CHECKFMT1;
RPPCAPI const char* _LogFuncname(const char* longFuncName); // shortens the func name

namespace rpp
{
    /**
     * @brief Returns the filename part of a long file path.
     * @param path The full file path
     * @return The filename part of the path
     */
    constexpr inline const char* shorten_filename(const char* path) noexcept
    {
        if (path == nullptr) return "(null)";
        const char* filename = path;
        for (const char* p = path; *p; ++p)
            if (*p == '/' || *p == '\\')
                filename = p + 1;
        return filename;
    }
}

#ifndef QUIETLOG
#define __log_format(format, file, line, func) "%s:%d %s $ " format, rpp::shorten_filename(file), line, _LogFuncname(func)
#else
#define __log_format(format, file, line, func) "$ " format
#endif

#if __cplusplus

// std::string logging compatibility
#include <string>

// MacOS Obj-C compatbility
#if __APPLE__ && __OBJC__
#import <Foundation/NSString.h>
#endif

// Qt compatibility
#if RPP_HAS_QT
#  include <QString>
#endif

namespace rpp
{
    template<>
    struct __wrap<const char*>
    { FINLINE static constexpr const char* w(const char* arg) noexcept { return arg; } };

    template<>
    struct __wrap<std::string>
    {
        // constexpr c_str() available in C++20 and up, but not on GCC C++20
        #if RPP_HAS_CXX23 || (RPP_HAS_CXX20 && __cpp_lib_constexpr_string && !RPP_GCC)
            FINLINE static constexpr const char* w(const std::string& arg) noexcept { return arg.c_str(); }
        #else
            FINLINE static const char* w(const std::string& arg) noexcept { return arg.c_str(); }
        #endif
    };

    #if __APPLE__ && __OBJC__
        template<>
        struct __wrap<NSString>
        { FINLINE static const char* w(NSString* arg) noexcept { return arg.UTF8String; } };
    #endif

    #if RPP_HAS_QT
        // Qt compatibility adapter -- it has to allocate a temporary array
        struct QtPrintable : public QByteArray
        {
            /*implicit*/QtPrintable(const QString& s) : QByteArray{s.toUtf8()} {}
            /*implicit*/QtPrintable(const QStringView& s) : QByteArray{s.toUtf8()} {}
        };
        template<>
        struct __wrap<QString>
        { FINLINE static const char* w(const QtPrintable& s) noexcept { return s.constData(); } };
    #endif

    // Helper to remove references and cv-qualifiers
    template<typename T>
    using __clean_type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    ///////////////////////////////////////////////////////////////////////////////////
    /// New C++ Logging API
    ///////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Handles a log message with the given severity level.
     * @param context User provided context pointer
     * @param severity Log severity level
     * @param message Log message
     * @param len Length of the message
     */
    using LogMsgHandler = void (*)(void* context, LogSeverity severity, const char* message, int len);

    /**
     * @brief Adds an additional log handler that is able to receive log messages
     *        when standard LogInfo(), LogWarning(), LogError() are called
     */
    void add_log_handler(void* context, LogMsgHandler handler) noexcept;

    /**
     * @brief Removes a matching log handler to prevent further logging calls
     *        on the `context` object.
     */
    void remove_log_handler(void* context, LogMsgHandler handler) noexcept;
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

#else // C:
  #define _rpp_wrap_args(...) , ##__VA_ARGS__
#endif


// wraps and formats message string for assertions, std::string and rpp::strview are wrapped and .c_str() called
#define _rpp_assert_format(fmt, ...) _FmtString(fmt _rpp_wrap_args(__VA_ARGS__) )


#if defined __APPLE__ || defined __clang__ // iOS or just clang
#  if __ANDROID__
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
#if !__ANDROID__
#  include <stdexcept>
#endif

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

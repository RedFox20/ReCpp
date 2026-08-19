#pragma once
/**
 * Cross platform debugging interface, Copyright (c) 2017-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include "log_colors.h" // re-export, consumers color their own log text
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
    LogSeverityInfo=0,  // merely information;
    LogSeverityWarn=1,  // warning unexpected behaviour; -- but we can recover
    LogSeverityError=2, // critical error or bug; -- a spectacular failure
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
 * @param time_of_day[false] If true, show the time of day, otherwise shows datetime.
 */
RPPCAPI void LogEnableTimestamps(bool enable, int precision = 3, bool time_of_day = false) noexcept;

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
     * @param path The full file path
     * @return The filename part of a long file path.
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

#if __cplusplus

// std::string logging compatibility
#if !RPP_BARE_METAL
#include <string>
#endif

#include <type_traits>

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

    #if !RPP_BARE_METAL
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
    #endif

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

// ThrowErr and AssertEx throw std::runtime_error. The macro header leaves this include
// to its user, so this header supplies it and every classic consumer keeps working.
#if !RPP_ANDROID && !RPP_BARE_METAL
#  include <stdexcept>
#endif

#endif // __cplusplus

// Every macro of this header lives in debugging.macros.h, because a module cannot export one.
// An importer of rpp.debugging includes that header directly instead of this one.
#include "debugging.macros.h" // re-export

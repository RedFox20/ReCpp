#include "debugging.h"
#include "timer.h" // rpp::TimePoint
#include "stack_trace.h"
#include "mutex.h"

#include <cstdio>
#include <cstring>
#include <cstdlib> // alloca

#if __ANDROID__
# include <android/log.h>
#endif

#if _WIN32
# define WIN32_LEAN_AND_MEAN 1
# include <Windows.h>
#endif

#if __APPLE__
# include <TargetConditionals.h>
#endif

#if __linux || TARGET_OS_OSX
# include <unistd.h>
#endif

#if __GNUC__ || __clang__
# include <signal.h>
#endif


#ifdef QUIETLOG
    static LogSeverity Filter = LogSeverityWarn;
#else
    static LogSeverity Filter = LogSeverityInfo;
#endif
static LogMessageCallback LogHandler;
static LogEventCallback EventHandler;
static LogExceptCallback ExceptHandler;
static bool DisableFunctionNames = false;
static bool EnableTimestamps = false;
static int TimePrecision = 3;
static rpp::int64 TimeOffset;

RPPCAPI void SetLogHandler(LogMessageCallback loghandler)
{
    LogHandler = loghandler;
}
RPPCAPI void SetLogErrorHandler(LogMessageCallback loghandler)
{
    LogHandler = loghandler;
}
RPPCAPI LogMessageCallback GetLogHandler()
{
    return LogHandler;
}
RPPCAPI void SetLogEventHandler(LogEventCallback eventHandler)
{
    EventHandler = eventHandler;
}
RPPCAPI void SetLogExceptHandler(LogExceptCallback exceptHandler)
{
    ExceptHandler = exceptHandler;
}
RPPCAPI void LogDisableFunctionNames()
{
    DisableFunctionNames = true;
}
RPPCAPI void SetLogSeverityFilter(LogSeverity filter)
{
    Filter = filter;
}
RPPCAPI LogSeverity GetLogSeverityFilter()
{
    return Filter;
}
RPPCAPI void LogEnableTimestamps(bool enable, int precision)
{
    EnableTimestamps = enable;
    TimePrecision = precision;
}
RPPCAPI void LogSetTimeOffset(rpp::int64 offset)
{
    TimeOffset = offset;
}

static int SafeFormat(char* errBuf, int N, const char* format, va_list ap) noexcept
{
    char* pbuf = errBuf;
    int remaining = N;
    int len = 0;
    if (EnableTimestamps) {
        rpp::TimePoint now = rpp::TimePoint::now();
        now.duration.nsec += TimeOffset;
        len = now.to_string(pbuf, remaining-1, TimePrecision);
        pbuf[len++] = ' '; // add separator
        pbuf += len;
        remaining -= len;
    }

    int plen = vsnprintf(pbuf, size_t(remaining-1)/*spare room for \n*/, format, ap);
    if (plen < 0 || plen >= remaining) { // err: didn't fit
        plen = remaining-2; // try to recover gracefully
        pbuf[plen] = '\0';
    }
    len += plen;

    return len;
}

#if __linux__
// split at $ and remove /long/path/ leaving just "filename.cpp:123 func() $ message"
static void ShortFilePathMessage(char*& ptr, int& len) noexcept
{
    if (char* middle = strchr(ptr, '$')) {
        for (; middle > ptr; --middle) {
            if (*middle == '/' || *middle == '\\') {
                ++middle;
                break;
            }
        }
        len = int((ptr + len) - middle);
        ptr = middle;
        ptr[len] = '\0';
    }
}
#endif

RPPCAPI RPP_NORETURN void RppAssertFail(const char* message, const char* file,
                                        unsigned int line, const char* function)
{
    _LogError("%s:%u %s: Assertion failed: %s", file, line, function, message);
    // show a nice stack trace if possible
    rpp::print_trace();
    fflush(stderr);

    // trap into debugger
    #if __clang__
        #if __has_builtin(__builtin_debugtrap)
            __builtin_debugtrap();
        #else
            raise(SIGTRAP);
        #endif
    #elif _MSC_VER
        __debugbreak();
    #elif __GNUC__
        raise(SIGTRAP);
    #endif

    std::terminate();
}

// this ensures default output newline is atomic with the rest of the error string
#define AllocaPrintlineBuf(err, len) \
    auto* buf = (char*)alloca(size_t(len) + 2); \
    memcpy(buf, err, size_t(len)); \
    buf[len] = '\n'; \
    buf[size_t(len)+1] = '\0';

RPPCAPI void LogWriteToDefaultOutput(const char* tag, LogSeverity severity, const char* err, int len)
{
    #if __ANDROID__
        (void)len;
        auto priority = severity == LogSeverityInfo ? ANDROID_LOG_INFO :
                        severity == LogSeverityWarn ? ANDROID_LOG_WARN :
                                                      ANDROID_LOG_ERROR;
        __android_log_write(priority, tag, err);
    #elif _WIN32
        static HANDLE winstd = GetStdHandle(STD_OUTPUT_HANDLE);
        static HANDLE winerr = GetStdHandle(STD_ERROR_HANDLE);
        static const WORD colormap[] = {
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY, // white - Info
            FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY, // bright yellow - Warning
            FOREGROUND_RED | FOREGROUND_INTENSITY, // bright red - Error
        };
        HANDLE winout = severity == LogSeverityError ? winstd : winerr;
        FILE*  fout   = severity == LogSeverityError ? stderr : stdout;

        AllocaPrintlineBuf(err, len);
        static rpp::mutex consoleSync;
        {
            std::lock_guard lock { consoleSync };
            SetConsoleTextAttribute(winout, colormap[severity]);
            fwrite(buf, size_t(len + 1), 1, fout); // fwrite to sync with unix-like shells
            SetConsoleTextAttribute(winout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }
        fflush(fout); // flush needed for proper sync with unix-like shells
    #elif __linux || TARGET_OS_OSX
        AllocaPrintlineBuf(err, len);
        FILE* fout = severity == LogSeverityError ? stderr : stdout;
        if (isatty(fileno(fout))) // is terminal?
        {
            if (severity)
            {
                static const char clearColor[] = "\x1b[0m"; // Default: clear all formatting
                static const char* colors[] = {
                        "\x1b[0m",  // Default
                        "\x1b[93m", // Warning: bright yellow
                        "\x1b[91m", // Error  : bright red
                };
                static rpp::mutex consoleSync;
                std::unique_lock guard{ consoleSync, std::defer_lock };
                if (consoleSync.native_handle()) guard.lock(); // lock if mutex not destroyed

                fwrite(colors[severity], strlen(colors[severity]), 1, fout);
                fwrite(buf, size_t(len) + 1, 1, fout);
                fwrite(clearColor, sizeof(clearColor)-1, 1, fout);
            }
            else
            {
                fwrite(buf, size_t(len) + 1, 1, fout);
            }
        }
        else
        {
            fwrite(buf, size_t(len) + 1, 1, fout);
        }
        fflush(fout); // flush needed for proper sync with different shells
    #else
        AllocaPrintlineBuf(err, len);
        FILE* fout = severity == LogSeverityError ? stderr : stdout;
        fwrite(buf, size_t(len) + 1, 1, fout);
        fflush(fout); // flush needed for proper sync with unix-like shells
    #endif
    (void)tag;
}

RPPCAPI void LogEventToDefaultOutput(const char* tag, const char* eventName, const char* message, int len)
{
    #if __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, tag, "EVT %s: %.*s", eventName, len, message);
    #else
        printf("EVT %s: %.*s\n", eventName, len, message);
    #endif
    (void)tag;
}

RPPCAPI void LogFormatv(LogSeverity severity, const char* format, va_list ap)
{
    if (severity < Filter)
        return;

    char errBuf[4096];
    int len = SafeFormat(errBuf, sizeof(errBuf), format, ap);

    if (LogHandler)
    {
        char* ptr = errBuf;
        #ifdef __linux__
          ShortFilePathMessage(ptr, len);
        #endif
        LogHandler(severity, ptr, len);
    }
    else
    {
        LogWriteToDefaultOutput("ReCpp", severity, errBuf, len);
    }
}


RPPCAPI void LogWrite(LogSeverity severity, const char* message, int len)
{
    if (severity < Filter)
        return;
    if (LogHandler)
        LogHandler(severity, message, len);
    else
        LogWriteToDefaultOutput("ReCpp", severity, message, len);
}

#define WrappedLogFormatv(severity, format) \
    va_list ap; va_start(ap, format); \
    LogFormatv(severity, format, ap); \
    va_end(ap);
RPPCAPI void _LogInfo(PRINTF_FMTSTR const char* format, ...) {
    WrappedLogFormatv(LogSeverityInfo, format);
}
RPPCAPI void _LogWarning(PRINTF_FMTSTR const char* format, ...) {
    WrappedLogFormatv(LogSeverityWarn, format);
}
RPPCAPI void _LogError(PRINTF_FMTSTR const char* format, ...) {
    WrappedLogFormatv(LogSeverityError, format);
}

RPPCAPI void LogEvent(const char* eventName, PRINTF_FMTSTR const char* format, ...)
{
#if __clang__
#if __has_feature(address_sanitizer)
    return; // ASAN reports a false positive with vsnprintf
#endif
#endif
    va_list ap; va_start(ap, format);
    char messageBuf[4096];
    int len = SafeFormat(messageBuf, sizeof(messageBuf), format, ap);
    va_end(ap);

    if (EventHandler)
    {
        EventHandler(eventName, messageBuf, len);
    }
    else
    {
        LogEventToDefaultOutput("ReCpp", eventName, messageBuf, len);
    }
}

void _LogExcept(const char* exceptionWhat, PRINTF_FMTSTR const char* format, ...)
{
#if __clang__
#if __has_feature(address_sanitizer)
    return; // ASAN reports a false positive with vsnprintf
#endif
#endif
    va_list ap; va_start(ap, format);
    char messageBuf[4096];
    int len = SafeFormat(messageBuf, sizeof(messageBuf), format, ap);
    va_end(ap);

    // only MSVC requires stderr write; other platforms do it in assert
#if _MSC_VER
    fprintf(stderr, "%.*s: %s\n", len, messageBuf, exceptionWhat);
#else
    (void)len;
#endif

    if (ExceptHandler != nullptr)
    {
        ExceptHandler(messageBuf, exceptionWhat);
    }
}

RPPCAPI const char* _FmtString(PRINTF_FMTSTR const char* format, ...)
{
    static thread_local char errBuf[4096];
    va_list ap; va_start(ap, format);
    SafeFormat(errBuf, sizeof(errBuf), format, ap);
    va_end(ap);
    return errBuf;
}

RPPCAPI const char* _LogFilename(const char* longFilePath)
{
    if (longFilePath == nullptr) return "(null)";
    const char* eptr = longFilePath + strlen(longFilePath);
    for (int n = 1; longFilePath < eptr; --eptr)
        if (*eptr == '/' || *eptr == '\\')
            if (--n <= 0) return ++eptr;
    return eptr;
}


static const int funcname_max = 48;

struct funcname_builder
{
    char* buffer;
    const char* ptr;
    int len = 0;

    explicit funcname_builder(char* buffer, const char* original)
        : buffer{buffer}, ptr{original-1} {}

    char read_next() { return len < funcname_max ? *++ptr : char('\0'); }

    template<size_t N, size_t M> bool replace(const char (&what)[N], const char (&with)[M])
    {
        if (memcmp(ptr, what, N - 1) == 0)
        {
            memcpy(&buffer[len], with, M - 1);
            len += M - 1;
            return true;
        }
        return false;
    }
    template<size_t N> bool skip(const char (&what)[N])
    {
        if (memcmp(ptr, what, N - 1) == 0) {
            ptr += N - 2; // - 2 because next() will bump the pointer
            return true;
        }
        return false;
    }

    void append(char ch) { buffer[len++] = ch; }
};

RPPCAPI const char* _LogFuncname(const char* longFuncName)
{
    if (DisableFunctionNames) return "";
    if (longFuncName == nullptr) return "(null)";

    // always skip the first ::
    const char* ptr = strchr(longFuncName, ':');
    if (ptr != nullptr) {
        if (*++ptr == ':') ++ptr;
    } else ptr = longFuncName;

    static thread_local char funcname_buf[64];

    funcname_builder fb { funcname_buf, ptr };
    while (char ch = fb.read_next())
    {
        if (ch == '<') {
            // replace invoke<<lambda_....>&> with just invoke<lambda>
            if (fb.replace("<<lambda", "<lambda>")) break; // no idea how long lambda, so stop here
            if (fb.replace("<lambda", "lambda"))    break;
        }

        // clean all std:: symbols
        if (ch == 's' && fb.skip("std::")) continue;

    #if !_MSC_VER // also skip __1:: to clean the symbols on clang
        if (ch == '_' && fb.skip("__1::")) continue;
    #else
        if (ch == ' ' && fb.skip(" __cdecl")) continue;
    #endif

        fb.append(ch);
    }

    if (fb.buffer[fb.len-1] == ']')
        --fb.len; // remove Objective-C method ending bracket
    fb.append('\0');
    return fb.buffer;
}

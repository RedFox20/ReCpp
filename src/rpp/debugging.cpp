#include "debugging.h"
#include "timer.h" // rpp::TimePoint
#include "stack_trace.h"
#include "strview.h"

#include <array>
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

static constexpr int MAX_LOG_HANDLERS = 16;
struct LogHandler
{
    void* context;
    rpp::LogMsgHandler handler;
};
static int NumLogHandlers;
static std::array<LogHandler, MAX_LOG_HANDLERS> LogHandlers;
static LogExceptCallback ExceptHandler;
static bool DisableFunctionNames = false;
static bool EnableTimestamps = false;
static bool TimeOfDay = false;
static int TimePrecision = 3;
static rpp::int64 TimeOffset;

// new logging API
namespace rpp
{
    static int index_of(const LogHandler& handler) noexcept
    {
        for (int i = 0; i < NumLogHandlers; ++i)
        {
            const LogHandler& h = LogHandlers[i];
            if (h.context == handler.context && h.handler == handler.handler)
                return i;
        }
        return -1;
    }

    static int index_of(LogMsgHandler handler_func) noexcept
    {
        for (int i = 0; i < NumLogHandlers; ++i)
            if (LogHandlers[i].handler == handler_func)
                return i;
        return -1;
    }

    void add_log_handler(void* context, LogMsgHandler handler) noexcept
    {
        if (NumLogHandlers < MAX_LOG_HANDLERS)
        {
            if (index_of({ context, handler }) == -1)
            {
                LogHandlers[NumLogHandlers++] = { context, handler };
            }
        }
    }

    void remove_log_handler(void* context, LogMsgHandler handler) noexcept
    {
        int index = index_of({ context, handler });
        if (index != -1)
        {
            int newSize = --NumLogHandlers;
            for (int i = index; i < newSize; ++i) // unshift, preserving order
                LogHandlers[i] = LogHandlers[i + 1];
        }
    }

    void remove_log_handler_func(LogMsgHandler handler_func) noexcept
    {
        int index = index_of(handler_func);
        if (index != -1)
        {
            int newSize = --NumLogHandlers;
            for (int i = index; i < newSize; ++i) // unshift, preserving order
                LogHandlers[i] = LogHandlers[i + 1];
        }
    }
}

// old-style API adapter
static void LogHandlerProxy(void* context, LogSeverity severity, const char* message, int len)
{
    LogMessageCallback old_callback = reinterpret_cast<LogMessageCallback>(context);
    old_callback(severity, message, len);
}

RPPCAPI void SetLogHandler(LogMessageCallback loghandler) noexcept
{
    // always remove the current proxy, since we can only have one at a time
    rpp::remove_log_handler_func(&LogHandlerProxy);
    if (loghandler)
    {
        rpp::add_log_handler(reinterpret_cast<void*>(loghandler), &LogHandlerProxy);
    }
}
RPPCAPI void SetLogExceptHandler(LogExceptCallback exceptHandler) noexcept
{
    ExceptHandler = exceptHandler;
}
RPPCAPI void LogDisableFunctionNames() noexcept
{
    DisableFunctionNames = true;
}
RPPCAPI void SetLogSeverityFilter(LogSeverity filter) noexcept
{
    Filter = filter;
}
RPPCAPI LogSeverity GetLogSeverityFilter() noexcept
{
    return Filter;
}
RPPCAPI void LogEnableTimestamps(bool enable, int precision, bool time_of_day) noexcept
{
    EnableTimestamps = enable;
    TimePrecision = precision;
    TimeOfDay = time_of_day;
}
RPPCAPI void LogSetTimeOffset(rpp::int64 offset) noexcept
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
        if (TimeOfDay)
            len = now.time_of_day().to_string(pbuf, remaining-1, TimePrecision);
        else
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

RPPCAPI void LogWriteToDefaultOutput(const char* tag, LogSeverity severity, const char* str, int len)
{
    #if __ANDROID__
        (void)len;
        auto priority = severity == LogSeverityInfo ? ANDROID_LOG_INFO :
                        severity == LogSeverityWarn ? ANDROID_LOG_WARN :
                                                      ANDROID_LOG_ERROR;
        __android_log_write(priority, tag, str);
    #else

        #if _MSC_VER
            // windows: configure code page as UTF-8, so any UTF-8 characters are printed correctly
            static bool have_configured_mode;
            if (!have_configured_mode)
            {
                have_configured_mode = true;
                SetConsoleOutputCP(CP_UTF8);
            }
        #endif

        FILE* cout = severity == LogSeverityError ? stderr : stdout;
        static constexpr rpp::strview colors[] = {
            "\x1b[0m",  // Default
            "\x1b[93m", // Warning: bright yellow
            "\x1b[91m", // Error  : bright red
        };
        int color = severity == LogSeverityInfo ? 0 :
                    severity == LogSeverityWarn ? 1 :
                                                  2 ;

        // perform a double copy to avoid having to mutex lock
        constexpr rpp::strview clear = colors[0];

        size_t total_len = len + 1/*newline*/;
        if (color != 0)
            total_len += colors[color].len + clear.len;

        // copy the string with color codes and newline
        auto* buf = (char*)alloca(total_len);
        size_t offset = 0;

        if (color != 0) // insert starting color code
        {
            rpp::strview c1 = colors[color];
            memcpy(&buf[offset], c1.str, c1.len);
            offset += c1.len;
        }

        memcpy(&buf[offset], str, len); // append log text itself
        offset += len;

        if (color != 0) // append clear color code
        {
            memcpy(&buf[offset], clear.str, clear.len);
            offset += clear.len;
        }

        buf[offset++] = '\n'; // newline

        fwrite(buf, total_len, 1, cout);
    #endif
    (void)tag;
}

RPPCAPI void LogFormatv(LogSeverity severity, const char* format, va_list ap)
{
    if (severity < Filter)
        return;

    char errBuf[4096];
    int len = SafeFormat(errBuf, sizeof(errBuf), format, ap);

    // truncate long filepaths on linux
    char* ptr = errBuf;
    #ifdef __linux__
        ShortFilePathMessage(ptr, len);
    #endif

    if (int num_handlers = NumLogHandlers)
    {
        for (int i = 0; i < num_handlers; ++i)
        {
            auto& h = LogHandlers[i];
            h.handler(h.context, severity, ptr, len);
        }
    }
    else
    {
        LogWriteToDefaultOutput("ReCpp", severity, ptr, len);
    }
}


RPPCAPI void LogWrite(LogSeverity severity, const char* message, int len)
{
    if (severity < Filter)
        return;
    if (int num_handlers = NumLogHandlers)
    {
        for (int i = 0; i < num_handlers; ++i)
        {
            auto& h = LogHandlers[i];
            h.handler(h.context, severity, message, len);
        }
    }
    else
    {
        LogWriteToDefaultOutput("ReCpp", severity, message, len);
    }
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

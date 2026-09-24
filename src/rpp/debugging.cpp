#include "debugging.h"
#include "timer.h" // rpp::TimePoint
#include "stack_trace.h"
#include "strview.h"
#include "threads.h" // rpp::yield
#if RPP_CORTEX_M_ARCH
# include "mutex.h" // rpp::critical_section
#endif

#include <atomic>
#include <array>
#include <cstdio>
#include <cstring>
#include <cstdlib> // alloca
#include <exception> // std::terminate

#if RPP_ANDROID
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
# include <csignal>
#endif

#if RPP_BARE_METAL
# if RPP_USE_EYALROZ_PRINTF
#  include <printf/printf.h>
# else
#  ifndef printf_
#   define printf_ printf
#  endif
# endif
# if RPP_STM32_HAL
#  include RPP_STM32_HAL_H
# endif
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
struct LogHandlerList
{
    int size = 0;
    std::array<LogHandler, MAX_LOG_HANDLERS> items;
};
// a dispatch never waits: an edit writes the list no dispatch reads, then publishes it
static LogHandlerList LogHandlerLists[2];
static std::atomic<LogHandlerList*> LogHandlers {&LogHandlerLists[0]};
static std::atomic_int LogDispatches[2] {}; // the dispatches on each list, so an edit waits only for the list it replaced
static std::atomic_bool LogHandlersEditing {false};
static LogExceptCallback ExceptHandler;
static std::atomic_bool DisableFunctionNames {false};
static std::atomic_bool EnableTimestamps {false};
static std::atomic_bool TimeOfDay {false};
static std::atomic_int TimePrecision {3};
#if RPP_CORTEX_M_ARCH
    // no 64-bit atomics on Cortex systems
    static rpp::int64 TimeOffset {0};
    static rpp::critical_section LogMutex;
#else
    static std::atomic<rpp::int64> TimeOffset {0};
#endif

// new logging API
namespace rpp
{
    static int index_of(const LogHandlerList& list, const LogHandler& handler) noexcept
    {
        for (int i = 0; i < list.size; ++i)
        {
            const LogHandler& h = list.items[i];
            if (h.context == handler.context && h.handler == handler.handler)
                return i;
        }
        return -1;
    }

    static int index_of(const LogHandlerList& list, LogMsgHandler handler_func) noexcept
    {
        for (int i = 0; i < list.size; ++i)
            if (list.items[i].handler == handler_func)
                return i;
        return -1;
    }

    // returns a copy of the published list, in the buffer which the last publish drained
    static LogHandlerList& edit_log_handlers() noexcept
    {
        while (LogHandlersEditing.exchange(true, std::memory_order_acquire))
            rpp::yield();
        LogHandlerList* published = LogHandlers.load(std::memory_order_relaxed);
        LogHandlerList& edit = LogHandlerLists[published == &LogHandlerLists[0] ? 1 : 0];
        edit = *published;
        return edit;
    }

    static void publish_log_handlers(LogHandlerList& edit) noexcept
    {
        // seq_cst pairs with dispatch_log(): either this drain counts a dispatch on `replaced`, or the dispatch loads `edit`
        LogHandlerList* replaced = LogHandlers.exchange(&edit, std::memory_order_seq_cst);
        while (LogDispatches[replaced - LogHandlerLists].load(std::memory_order_seq_cst) != 0)
            rpp::yield(); // a dispatch can still run a removed handler, and its owner frees the context next
        LogHandlersEditing.store(false, std::memory_order_release);
    }

    static void remove_at(LogHandlerList& list, int index) noexcept
    {
        --list.size;
        for (int i = index; i < list.size; ++i) // unshift, preserving order
            list.items[i] = list.items[i + 1];
    }

    void add_log_handler(void* context, LogMsgHandler handler) noexcept
    {
        LogHandlerList& edit = edit_log_handlers();
        if (handler && edit.size < MAX_LOG_HANDLERS && index_of(edit, { context, handler }) == -1)
            edit.items[edit.size++] = { context, handler };
        publish_log_handlers(edit);
    }

    void remove_log_handler(void* context, LogMsgHandler handler) noexcept
    {
        LogHandlerList& edit = edit_log_handlers();
        if (int index = index_of(edit, { context, handler }); index != -1)
            remove_at(edit, index);
        publish_log_handlers(edit);
    }

}

// calls every handler of the published list, and returns false when the list is empty
static FINLINE bool dispatch_log(LogSeverity severity, const char* message, int len) noexcept
{
    const LogHandlerList* list = LogHandlers.load(std::memory_order_seq_cst);
    for (;;)
    {
        LogDispatches[list - LogHandlerLists].fetch_add(1, std::memory_order_seq_cst);
        const LogHandlerList* published = LogHandlers.load(std::memory_order_seq_cst);
        if (published == list)
            break;
        // an edit published the other list, maybe before this count, so its drain can miss this dispatch
        LogDispatches[list - LogHandlerLists].fetch_sub(1, std::memory_order_release);
        list = published;
    }
    const int size = list->size;
    for (int i = 0; i < size; ++i)
        list->items[i].handler(list->items[i].context, severity, message, len);
    LogDispatches[list - LogHandlerLists].fetch_sub(1, std::memory_order_release);
    return size != 0;
}

// old-style API adapter
static void LogHandlerProxy(void* context, LogSeverity severity, const char* message, int len)
{
    LogMessageCallback old_callback = reinterpret_cast<LogMessageCallback>(context);
    old_callback(severity, message, len);
}

RPPCAPI void SetLogHandler(LogMessageCallback loghandler) noexcept
{
    // one edit replaces the proxy, so two setters cannot leave two proxies behind
    LogHandlerList& edit = rpp::edit_log_handlers();
    if (int index = rpp::index_of(edit, &LogHandlerProxy); index != -1)
        rpp::remove_at(edit, index);
    if (loghandler && edit.size < MAX_LOG_HANDLERS)
        edit.items[edit.size++] = { reinterpret_cast<void*>(loghandler), &LogHandlerProxy };
    rpp::publish_log_handlers(edit);
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
#if RPP_CORTEX_M_ARCH
    std::lock_guard<rpp::critical_section> guard{LogMutex};
    TimeOffset = offset;
#else
    TimeOffset.store(offset, std::memory_order_release);
#endif
}

static int SafeFormat(char* errBuf, int N, const char* format, va_list ap) noexcept
{
    char* pbuf = errBuf;
    int remaining = N;
    int len = 0;

    if (EnableTimestamps) {
        rpp::TimePoint now = rpp::TimePoint::system_now();
    #if RPP_CORTEX_M_ARCH
        {
            std::lock_guard<rpp::critical_section> guard{LogMutex};
            now.duration.nsec += TimeOffset;
        }
    #else
        now.duration.nsec += TimeOffset.load(std::memory_order_relaxed);
    #endif
        if (TimeOfDay)
            len = now.time_of_day().to_string(pbuf, remaining-1, TimePrecision);
        else
            len = now.to_string(pbuf, remaining-1, TimePrecision);
        pbuf[len++] = ' '; // add separator
        pbuf += len;
        remaining -= len;
    }

    int plen = vsnprintf(pbuf, size_t(remaining-1)/*spare room for \n*/, format, ap); // NOLINT(clang-analyzer-valist.Uninitialized)

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
    #if !RPP_BARE_METAL && ENABLE_STACK_TRACE
        rpp::print_trace();
        fflush(stderr);
    #endif

    // trap into debugger
    // TODO: replace with std::breakpoint() when we switch to C++26
    #if RPP_BARE_METAL && RPP_ARM_ARCH
        __asm__ volatile ("bkpt #0");
    #elif __clang__
        #if __has_builtin(__builtin_debugtrap)
            __builtin_debugtrap();
        #else
            raise(SIGTRAP);
        #endif
    #elif _MSC_VER || __MINGW32__
        __debugbreak();
    #elif __GNUC__
        raise(SIGTRAP);
    #endif

    std::terminate();
}

RPPCAPI void LogWriteToDefaultOutput(const char* tag, LogSeverity severity, const char* str, int len)
{
    #if RPP_ANDROID
        (void)len;
        auto priority = ANDROID_LOG_ERROR;
        if      (severity == LogSeverityInfo) priority = ANDROID_LOG_INFO;
        else if (severity == LogSeverityWarn) priority = ANDROID_LOG_WARN;
        __android_log_write(priority, tag, str);
    #elif RPP_BARE_METAL
        printf_("%.*s\n", len, str);
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
        int color = severity == LogSeverityInfo ? 0 : // NOLINT
                    severity == LogSeverityWarn ? 1 : // NOLINT
                                                  2 ; // NOLINT

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

    if (!dispatch_log(severity, ptr, len))
        LogWriteToDefaultOutput("ReCpp", severity, ptr, len);
}


RPPCAPI void LogWrite(LogSeverity severity, const char* message, int len)
{
    if (severity < Filter)
        return;
    if (!dispatch_log(severity, message, len))
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
    va_list ap;
    va_start(ap, format);
    SafeFormat(errBuf, sizeof(errBuf), format, ap);
    va_end(ap);
    return errBuf;
}

static const int funcname_max = 48;

struct funcname_builder
{
    char buffer[64];
    const char* ptr;
    int len;

    void reset(const char* start)
    {
        len = 0;
        ptr = start;
    }

    char read_next() noexcept { return len < funcname_max ? *++ptr : '\0'; }
    void append(char ch) noexcept { buffer[len++] = ch; }

    template<size_t N, size_t M> bool replace(const char (&what)[N], const char (&with)[M]) noexcept
    {
        if (cur_ptr_equals<N>(what))
        {
            memcpy(&buffer[len], with, M - 1);
            len += M - 1;
            return true;
        }
        return false;
    }
    template<size_t N> bool skip(const char (&what)[N]) noexcept
    {
        if (cur_ptr_equals<N>(what)) {
            ptr += N - 2; // - 2 because next() will bump the pointer
            return true;
        }
        return false;
    }
    template<size_t N> bool cur_ptr_equals(const char (&what)[N]) const noexcept
    {
        for (const char* p = ptr, *w = what; (*p || *w); ++p, ++w)
            if (*p != *w) return false;
        return true;
    }
};

RPPCAPI const char* _LogFuncname(const char* longFuncName)
{
    if (DisableFunctionNames) return "";
    if (longFuncName == nullptr) return "(null)";

    static thread_local funcname_builder fb;

    // always skip the first ::
    if (const char* ptr = strchr(longFuncName, ':'))
    {
        if (*++ptr == ':') ++ptr;
        fb.reset(ptr - 1); // -1 because read_next() uses pre-increment
    }
    else
    {
        fb.reset(longFuncName - 1);
    }

    while (char ch = fb.read_next())
    {
        if (ch == '<')
        {
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

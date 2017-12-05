#include "debugging.h"
#include <cstdio>
#include <cstring>
#include <mutex>
#if __ANDROID__
  #include <android/log.h>
#endif
#if _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#endif

#ifdef QUIETLOG
static LogSeverity Filter = LogSeverityWarn;
#else
static LogSeverity Filter = LogSeverityInfo;
#endif
static LogErrorCallback ErrorHandler;
static LogEventCallback EventHandler;
static LogExceptCallback ExceptHandler;
static bool DisableFunctionNames = false;

EXTERNC void SetLogErrorHandler(LogErrorCallback errorHandler)
{
    ErrorHandler = errorHandler;
}
EXTERNC void SetLogEventHandler(LogEventCallback eventHandler)
{
    EventHandler = eventHandler;
}
EXTERNC void SetLogExceptHandler(LogExceptCallback exceptFunc)
{
    ExceptHandler = exceptFunc;
}
EXTERNC void LogDisableFunctionNames()
{
    DisableFunctionNames = true;
}
EXTERNC void SetLogSeverityFilter(LogSeverity filter)
{
    Filter = filter;
}
EXTERNC LogSeverity GetLogSeverityFilter()
{
    return Filter;
}

static int SafeFormat(char* errBuf, int N, const char* format, va_list ap)
{
    int len = vsnprintf(errBuf, size_t(N-1)/*spare room for \n*/, format, ap);
    if (len < 0 || len >= N) { // err: didn't fit
        len = N-2; // try to recover gracefully
        errBuf[len] = '\0';
    }
    return len;
}

#if __linux__
// split at $ and remove /long/path/ leaving just "filename.cpp:123 func() $ message"
static void ShortFilePathMessage(char*& ptr, int& len)
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

// this ensures default output newline is atomic with the rest of the error string
#define AllocaPrintlineBuf(err, len) \
    char* buf = (char*)alloca(len + 2); \
    memcpy(buf, err, len); \
    buf[len] = '\n'; \
    buf[len+1] = '\0';

EXTERNC void LogWriteToDefaultOutput(const char* tag, LogSeverity severity, const char* err, int len)
{
    #if __ANDROID__
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
        FILE*  cout   = severity == LogSeverityError ? stderr : stdout;

        AllocaPrintlineBuf(err, len);
        
        using namespace std;
        static mutex consoleSync;
        {
            lock_guard<mutex> guard{ consoleSync };
            SetConsoleTextAttribute(winout, colormap[severity]);
            fwrite(buf, size_t(len + 1), 1, cout); // fwrite to sync with unix-like shells
            SetConsoleTextAttribute(winout, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }
        fflush(cout); // flush needed for proper sync with unix-like shells

    #else
        AllocaPrintlineBuf(err, len);
        FILE* cout = (severity == LogSeverityError) ? stderr : stdout;
        fwrite(buf, size_t(len + 1), 1, cout);
        fflush(cout); // flush needed for proper sync with unix-like shells
    #endif
    (void)tag;
}

EXTERNC void LogEventToDefaultOutput(const char* tag, const char* eventName, const char* message, int len)
{
    #if __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, tag, "EVT %s: %.*s", eventName, len, message);
    #else
        printf("EVT %s: %.*s\n", eventName, len, message);
    #endif
    (void)tag;
}

EXTERNC void LogFormatv(LogSeverity severity, const char* format, va_list ap)
{
    if (severity < Filter)
        return;
    
    char errBuf[4096];
    int len = SafeFormat(errBuf, sizeof(errBuf), format, ap);
    
    if (ErrorHandler)
    {
        char* ptr = errBuf;
        #ifdef __linux__
          ShortFilePathMessage(ptr, len);
        #endif
        ErrorHandler(severity, ptr, len);
    }
    else
    {
        LogWriteToDefaultOutput("ReCpp", severity, errBuf, len);
    }
}

#define WrappedLogFormatv(severity, format) \
    va_list ap; va_start(ap, format); \
    LogFormatv(severity, format, ap); \
    va_end(ap);
EXTERNC void _LogInfo(const char* format, ...) {
    WrappedLogFormatv(LogSeverityInfo, format);
}
EXTERNC void _LogWarning(const char* format, ...) {
    WrappedLogFormatv(LogSeverityWarn, format);
}
EXTERNC void _LogError(const char* format, ...) {
    WrappedLogFormatv(LogSeverityError, format);
}

EXTERNC void LogEvent(const char* eventName, const char* format, ...)
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

void _LogExcept(const char* exceptionWhat, const char* format, ...)
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

EXTERNC const char* _FmtString(const char* format, ...)
{
    static thread_local char errBuf[4096];
    va_list ap; va_start(ap, format);
    SafeFormat(errBuf, sizeof(errBuf), format, ap);
    va_end(ap);
    return errBuf;
}

EXTERNC const char* _LogFilename(const char* longFilePath)
{
    if (longFilePath == nullptr) return "(null)";
    const char* eptr = longFilePath + strlen(longFilePath);
    for (int n = 1; longFilePath < eptr; --eptr)
        if (*eptr == '/' || *eptr == '\\')
            if (--n <= 0) return ++eptr;
    return eptr;
}

EXTERNC const char* _LogFuncname(const char* longFuncName)
{
    if (DisableFunctionNames) return "";
    if (longFuncName == nullptr) return "(null)";

    static thread_local char buf[64];
    static const int max = 36;

    // always skip the first ::
    const char* ptr = strchr(longFuncName, ':');
    if (ptr != nullptr) {
        if (*++ptr == ':') ++ptr;
    }
    else ptr = longFuncName;

    char ch;
    int len = 0;
    for (; len < max && (ch = *ptr) != '\0'; ++ptr) {
        if (ch == '<') {
            if (memcmp(ptr, "<<lambda", 8) == 0) { // MSVC std::invoke<<lambda_....>&>
                memcpy(&buf[len], "<lambda>", 8);
                len += 8;
                break;
            }
            if (memcmp(ptr, "<lambda", 7) == 0) {
                memcpy(&buf[len], "lambda", 6);
                len += 6;
                break;
            }
        }
        buf[len++] = ch;
    }
    if (buf[len-1] == ']')
        --len; // remove Objective-C method ending bracket
    buf[len] = '\0';
    return buf;
}

void Log(LogSeverity severity, rpp::strview message)
{
    if (severity < Filter)
        return;

    auto file = (severity == LogSeverityError) ? stderr : stdout;
    fwrite(message.str, (size_t)message.len, 1, file);
    fwrite("\n", 1, 1, file);

    if (ErrorHandler != nullptr)
    {
        ErrorHandler(severity, message.str, message.len);
    }
}

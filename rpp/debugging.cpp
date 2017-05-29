#include "debugging.h"
#include <cstdio>
#include <cstring>


#ifdef QUIETLOG
static LogSeverity Filter = LogSeverityWarn;
#else
static LogSeverity Filter = LogSeverityInfo;
#endif
static LogErrorCallback ErrorHandler;

EXTERNC void SetLogErrorHandler(LogErrorCallback errfunc)
{
    ErrorHandler = errfunc;
}
EXTERNC void SetLogSeverityFilter(LogSeverity filter)
{
    Filter = filter;
}

EXTERNC void LogFormatv(LogSeverity severity, const char* format, va_list ap)
{
    if (severity < Filter)
    {
        return;
    }
    
    char errBuf[4096];
    // always format, because return string is used in asserts
    int len = vsnprintf(errBuf, sizeof(errBuf)-1, format, ap);
    if (len < 0) { // err: didn't fit
        len = sizeof(errBuf)-2; // try to recover gracefully
        errBuf[len] = '\0';
    }
    errBuf[len++] = '\n'; // force append a newline
    
    // some default logging behavior:
    fwrite(errBuf, len, 1, (severity == LogSeverityError) ? stderr : stdout);

    // optional error handler for iOS Crashlytics logging
    if (ErrorHandler)
    {
        ErrorHandler(severity, errBuf, len);
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

EXTERNC const char* _FmtString(const char* format, ...) {
    static char errBuf[4096];

    va_list ap; va_start(ap, format);
    int len = vsnprintf(errBuf, sizeof(errBuf)-1, format, ap);
    
    if (len < 0) { // err: didn't fit
        len = sizeof(errBuf)-2; // try to recover gracefully
        errBuf[len] = '\0';
    }
    return errBuf;
}

EXTERNC const char* _LogFilename(const char* longFilePath)
{
    if (!longFilePath) return "(null)";
    const char* eptr = longFilePath + strlen(longFilePath);
    for (int n = 1; longFilePath < eptr; --eptr)
        if (*eptr == '/' || *eptr == '\\')
            if (--n <= 0) break;
    return eptr;
}

EXTERNC const char* _LogFuncname(const char* longFuncName)
{
    if (!longFuncName) return "(null)";

    static char buf[64];
    static const int max = 36;

    // always skip the first ::
    const char* ptr = (ptr = strchr(longFuncName, ':')) ? ptr+2 : longFuncName;

    char ch;
    int len = 0;
    for (; len < max && (ch = *ptr); ++ptr) {
        if (ch == '<' && memcmp(ptr, "<lambda", 7) == 0) {
            memcpy(&buf[len], "lambda", 6);
            len += 6;
            break;
        }
        buf[len++] = ch;
    }
    buf[len] = '\0';
    return buf;
}


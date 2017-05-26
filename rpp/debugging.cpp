#include "debugging.h"
#include <cstdio>
#include <cstring>


#ifdef QUIETLOG
static LogSeverity Filter = LogSeverityWarn;
#else
static LogSeverity Filter = LogSeverityInfo;
#endif
static LogErrorCallback ErrorHandler;
static char ErrBuf[8192];

EXTERNC void SetLogErrorHandler(LogErrorCallback errfunc)
{
    ErrorHandler = errfunc;
}
EXTERNC void SetLogSeverityFilter(LogSeverity filter)
{
    Filter = filter;
}

EXTERNC const char* LogFormatv(LogSeverity severity, const char* format, va_list ap)
{
    // always format, because return string is used in asserts
    int len = vsnprintf(ErrBuf, sizeof(ErrBuf)-1, format, ap);
    if (len < 0) { // err: didn't fit
        len = sizeof(ErrBuf)-2; // try to recover gracefully
        ErrBuf[len] = '\0';
    }
    ErrBuf[len++] = '\n'; // force append a newline
    
    if (severity >= Filter)
    {
        // some default logging behavior:
        fwrite(ErrBuf, len, 1, (severity == LogSeverityError) ? stderr : stdout);

        // optional error handler for iOS Crashlytics logging
        if (ErrorHandler)
        {
            ErrorHandler(severity, ErrBuf, len);
        }
    }
    return ErrBuf;
}

#define WrappedLogFormatv(severity, format) \
    va_list ap; va_start(ap, format); \
    const char* err = LogFormatv(severity, format, ap); \
    va_end(ap); return err;

EXTERNC const char* LogFormat(LogSeverity severity, const char* format, ...) {
    WrappedLogFormatv(severity, format);
}
EXTERNC const char* LogString(LogSeverity severity, const char* string) {
    return LogFormat(severity, "%s", string);
}
EXTERNC const char* _LogInfo(const char* format, ...) {
    WrappedLogFormatv(LogSeverityInfo, format);
}
EXTERNC const char* _LogWarning(const char* format, ...) {
    WrappedLogFormatv(LogSeverityWarn, format);
}
EXTERNC const char* _LogError(const char* format, ...) {
    WrappedLogFormatv(LogSeverityError, format);
}

EXTERNC const char* _FmtString(const char* format, ...) {
    va_list ap; va_start(ap, format);
    vsprintf(ErrBuf, format, ap);
    va_end(ap);
    return ErrBuf;
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


/**
 * Replacement liblog.so for QEMU user-mode testing.
 * Redirects __android_log_* to stdout/stderr with ANSI-colored priority labels.
 * ERROR/FATAL go to stderr, everything else to stdout.
 * Does not append extra newlines — callers already include them.
 *
 * Build: .circleci/build_liblog.sh
 */
#include <stdio.h>
#include <stdarg.h>

enum {
    ANDROID_LOG_VERBOSE = 2,
    ANDROID_LOG_DEBUG   = 3,
    ANDROID_LOG_INFO    = 4,
    ANDROID_LOG_WARN    = 5,
    ANDROID_LOG_ERROR   = 6,
    ANDROID_LOG_FATAL   = 7,
};

static const char* prio_color(int prio)
{
    switch (prio)
    {
        case ANDROID_LOG_VERBOSE: return "\x1b[0m";   // default
        case ANDROID_LOG_DEBUG:   return "\x1b[0m";   // default
        case ANDROID_LOG_INFO:    return "\x1b[32m";  // green
        case ANDROID_LOG_WARN:    return "\x1b[33m";  // yellow
        case ANDROID_LOG_ERROR:   return "\x1b[31m";  // red
        case ANDROID_LOG_FATAL:   return "\x1b[31m";  // red
        default:                  return "\x1b[0m";
    }
}

static const char* prio_label(int prio)
{
    switch (prio)
    {
        case ANDROID_LOG_VERBOSE: return "VERBOSE";
        case ANDROID_LOG_DEBUG:   return "DEBUG";
        case ANDROID_LOG_INFO:    return "INFO";
        case ANDROID_LOG_WARN:    return "WARN";
        case ANDROID_LOG_ERROR:   return "ERROR";
        case ANDROID_LOG_FATAL:   return "FATAL";
        default:                  return "?";
    }
}

static FILE* log_stream(int prio)
{
    return (prio >= ANDROID_LOG_ERROR) ? stderr : stdout;
}

static void print_prefix(FILE* out, int prio, const char* tag)
{
    fprintf(out, "%s%s/%s: \x1b[0m", prio_color(prio), prio_label(prio), tag);
}

int __android_log_write(int prio, const char* tag, const char* text)
{
    FILE* out = log_stream(prio);
    print_prefix(out, prio, tag);
    int ret = fprintf(out, "%s", text);
    fflush(out);
    return ret;
}

int __android_log_print(int prio, const char* tag, const char* fmt, ...)
{
    FILE* out = log_stream(prio);
    print_prefix(out, prio, tag);
    va_list ap;
    va_start(ap, fmt);
    int ret = vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
    fflush(out);
    return ret;
}

int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap)
{
    FILE* out = log_stream(prio);
    print_prefix(out, prio, tag);
    int ret = vfprintf(out, fmt, ap);
    fputc('\n', out);
    fflush(out);
    return ret;
}

void __android_log_assert(const char* cond, const char* tag, const char* fmt, ...)
{
    print_prefix(stderr, ANDROID_LOG_FATAL, tag);
    fprintf(stderr, "ASSERT FAILED");
    if (cond) fprintf(stderr, " [%s]", cond);
    if (fmt)
    {
        fprintf(stderr, ": ");
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
    }
    fputc('\n', stderr);
    fflush(stderr);
    __builtin_abort();
}

int __android_log_is_loggable(int prio, const char* tag, int default_prio)
{
    (void)prio; (void)tag; (void)default_prio;
    return 1;
}

int __android_log_buf_write(int bufID, int prio, const char* tag, const char* text)
{
    (void)bufID;
    return __android_log_write(prio, tag, text);
}

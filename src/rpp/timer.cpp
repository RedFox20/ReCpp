#include "timer.h"

#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#elif __ANDROID__
    #include <android/log.h>
#elif RPP_BARE_METAL
    #include <printf/printf.h>
#endif

#if defined(__has_include) && __has_include("debugging.h")
    #define HAS_DEBUGGING_H 1
    #include "debugging.h"
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////////

    Timer::Timer() noexcept : started{TimePoint::now()}
    {
    }

    Timer::Timer(StartMode mode) noexcept
        : started{mode == StartMode::AutoStart ? TimePoint::now() : TimePoint{}}
    {
    }

    double Timer::elapsed() const noexcept
    {
        TimePoint end = TimePoint::now();
        return started.elapsed_sec(end);
    }

    double Timer::elapsed_millis() const noexcept
    {
        return elapsed() * 1'000.0;
    }

    double Timer::elapsed_micros() const noexcept
    {
        return elapsed() * 1'000'000.0;
    }

    double Timer::next() noexcept
    {
        TimePoint now = TimePoint::now();
        double t = started.elapsed_sec(now);
        started = now;
        return t;
    }

    double Timer::next_millis() noexcept
    {
        return next() * 1000.0;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    ScopedPerfTimer::ScopedPerfTimer(const char* prefix, const char* location,
                                     const char* detail, unsigned long threshold_us) noexcept
        : prefix{prefix}
        , location{location}
        , detail{detail}
        , start{TimePoint::now()}
        , threshold_us{threshold_us}
    {
    }

    ScopedPerfTimer::~ScopedPerfTimer() noexcept
    {
        // measure elapsed time as the first operation in the destructor
        TimePoint now = TimePoint::now();
        Duration elapsed = start.elapsed(now);
        if (threshold_us != 0 && elapsed.micros() <= rpp::int64(threshold_us))
            return; // within threshold, don't report anything

        double elapsed_ms = elapsed.msec();

        const char* padDetail = detail ? " " : "";
        detail = detail ? detail : "";
        prefix = prefix ? prefix : "";

        #if !__ANDROID__ && HAS_DEBUGGING_H
            LogInfo("%s %s%s%s elapsed: %.3fms", prefix, location, padDetail, detail, elapsed_ms);
        #else
            constexpr const char format[] = "$ %s %s%s%s elapsed: %.3fms\n";
            #if _WIN32
                static HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
                SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN); // dark yellow
                fprintf(stderr, format, prefix, location, padDetail, detail, elapsed_ms);
                SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // default color
            #elif __ANDROID__
                __android_log_print(ANDROID_LOG_WARN, "rpp", format, prefix, location, padDetail, detail, elapsed_ms);
            #elif RPP_BARE_METAL
                printf_(format, prefix, location, padDetail, detail, elapsed_ms);
            #else // @todo Proper Linux & OSX implementations
                fprintf(stderr, format, prefix, location, padDetail, detail, elapsed_ms);
            #endif
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    void StopWatch::start() noexcept
    {
        if (!begin) {
            begin = TimePoint::now();
            end = {};
        }
    }

    void StopWatch::stop() noexcept
    {
        if (begin && !end)
            end = TimePoint::now();
    }

    void StopWatch::resume() noexcept
    {
        end = {};
    }

    void StopWatch::clear()noexcept
    {
        begin = {};
        end = {};
    }

    void StopWatch::restart() noexcept
    {
        begin = TimePoint::now();
        end = {};
    }

    double StopWatch::elapsed() const noexcept
    {
        if (begin) {
            if (end) {
                return begin.elapsed_sec(end);
            } else {
                return begin.elapsed_sec(TimePoint::now());
            }
        }
        return 0.0;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

} // rpp

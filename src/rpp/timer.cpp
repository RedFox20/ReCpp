#include "timer.h"

#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#elif RPP_ANDROID
    #include <android/log.h>
#elif RPP_BARE_METAL
    #if RPP_USE_EYALROZ_PRINTF
    #include <printf/printf.h>
    #else
    #include <cstdio>
    #ifndef printf_
    #define printf_ printf
    #endif
    #endif
#endif

#if defined(__has_include) && __has_include("debugging.h")
    #define HAS_DEBUGGING_H 1
    #include "debugging.h"
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////////

    Timer::Timer() noexcept : started{TimePoint::now(DefaultClock)}
    {
    }

    Timer::Timer(ClockType clock) noexcept
        : started{TimePoint::now(clock)}, clock{clock}
    {
    }

    Timer::Timer(StartMode mode) noexcept
        : started{mode == StartMode::AutoStart ? TimePoint::now(DefaultClock) : TimePoint{}}
    {
    }

    Timer::Timer(ClockType clock, StartMode mode) noexcept
        : started{mode == StartMode::AutoStart ? TimePoint::now(clock) : TimePoint{}}
        , clock{clock}
    {
    }

    double Timer::elapsed() const noexcept
    {
        TimePoint end = time_now();
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
        TimePoint now = time_now();
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
        , start{TimePoint::monotonic_now()}
        , threshold_us{threshold_us}
    {
    }

    ScopedPerfTimer::~ScopedPerfTimer() noexcept
    {
        // measure elapsed time as the first operation in the destructor
        TimePoint now = TimePoint::monotonic_now();
        Duration elapsed = start.elapsed(now);
        if (threshold_us != 0 && elapsed.micros() <= rpp::int64(threshold_us))
            return; // within threshold, don't report anything

        double elapsed_ms = elapsed.msec();

        const char* padDetail = detail ? " " : "";
        detail = detail ? detail : "";
        prefix = prefix ? prefix : "";

        #if !RPP_ANDROID && HAS_DEBUGGING_H
            LogInfo("%s %s%s%s elapsed: %.3fms", prefix, location, padDetail, detail, elapsed_ms);
        #else
            constexpr const char format[] = "$ %s %s%s%s elapsed: %.3fms"
                #if !RPP_ANDROID // no newlines on android
                "\n"
                #endif
                ;
            #if _WIN32
                static HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
                SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN); // dark yellow
                fprintf(stderr, format, prefix, location, padDetail, detail, elapsed_ms);
                SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // default color
            #elif RPP_ANDROID
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
            begin = TimePoint::monotonic_now();
            end = {};
        }
    }

    void StopWatch::stop() noexcept
    {
        if (begin && !end)
            end = TimePoint::monotonic_now();
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
        begin = TimePoint::monotonic_now();
        end = {};
    }

    double StopWatch::elapsed() const noexcept
    {
        if (begin) {
            if (end) {
                return begin.elapsed_sec(end);
            } else {
                return begin.elapsed_sec(TimePoint::monotonic_now());
            }
        }
        return 0.0;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

} // rpp

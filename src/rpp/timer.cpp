#include "timer.h"

#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <timeapi.h>
    #pragma comment(lib, "Winmm.lib") // MM time library
#elif __APPLE__ || __linux__ || __ANDROID__ || __EMSCRIPTEN__
    #include <stdio.h>  // fprintf
    #include <unistd.h> // usleep()
    #if __linux__ || __ANDROID__ 
        #include <sys/time.h>
        #if __ANDROID__
            #include <android/log.h>
        #endif
    #endif
    #include <thread>
#endif

#if defined(__has_include) && __has_include("debugging.h")
    #define HAS_DEBUGGING_H 1
    #include "debugging.h"
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////////

#if _WIN32
    // uses multimedia timer API-s to sleep more accurately
    static void win32_sleep_ns(uint64 nanos)
    {
        if (nanos == 0)
        {
            // sleep 0 is special, gives priority to other threads, otherwise returns immediately
            SleepEx(0, /*alertable*/TRUE);
            return;
        }

        MMRESULT timeBeginStatus = timeBeginPeriod(1); // set 1ms precision
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -int64(nanos / 100);

        if (HANDLE hTimer = CreateWaitableTimer(NULL, TRUE, NULL))
        {
            if (SetWaitableTimer(hTimer, &dueTime, 0, NULL, NULL, 0))
                WaitForSingleObject(hTimer, INFINITE);
            else
                SleepEx(static_cast<int>(nanos / 1'000'000), /*alertable*/TRUE);
            CloseHandle(hTimer);
        }
        else
        {
            SleepEx(static_cast<int>(nanos / 1'000'000), /*alertable*/TRUE);
        }

        if (timeBeginStatus == TIMERR_NOERROR)
        {
            timeEndPeriod(1);
        }
    }
#elif __APPLE__ || __linux__ || __EMSCRIPTEN__
    inline void unix_sleep_ns_abstime(uint64 nanos)
    {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec  += (nanos / NANOS_PER_SEC);
        deadline.tv_nsec += (nanos % NANOS_PER_SEC);
        // normalize tv_nsec by overflowing into tv_sec
        if (deadline.tv_nsec >= NANOS_PER_SEC) {
            deadline.tv_nsec -= NANOS_PER_SEC;
            deadline.tv_sec++;
        }
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &deadline, NULL);
    }
#endif

    void sleep_ms(unsigned int millis) noexcept
    {
        #if _WIN32
            win32_sleep_ns(millis * 1'000'000ull);
        #elif __APPLE__ || __linux__ || __EMSCRIPTEN__
            unix_sleep_ns_abstime(millis * 1'000'000ull);
        #else
            std::this_thread::sleep_for(std::chrono::milliseconds{millis});
        #endif
    }

    void sleep_us(unsigned int micros) noexcept
    {
        #if _WIN32
            win32_sleep_ns(micros * 1'000ull);
        #elif __APPLE__ || __linux__ || __EMSCRIPTEN__
            unix_sleep_ns_abstime(micros * 1'000ull);
        #else
            std::this_thread::sleep_for(std::chrono::microseconds{micros});
        #endif
    }

    void sleep_ns(uint64 nanos) noexcept
    {
        #if _WIN32
            win32_sleep_ns(nanos);
        #elif __APPLE__ || __linux__ || __EMSCRIPTEN__
            unix_sleep_ns_abstime(nanos);
        #else
            std::this_thread::sleep_for(std::chrono::nanoseconds{nanos});
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    static int time_to_string(int64 ns, char* buf, int bufsize) noexcept
    {
        using rpp::int64;
        int len = 0;

        if (ns < 0)
        {
            ns = -ns;
            buf[len++] = '-';
        }

        // NOTE: this is not accurate for leap years, but we don't want to display them anyway
        constexpr int64 NANOS_PER_YEAR = 365LL * rpp::NANOS_PER_DAY;
        int64 years   = (ns / NANOS_PER_YEAR);        ns -= years * NANOS_PER_YEAR;
        int64 days    = (ns / rpp::NANOS_PER_DAY);    ns -= days * rpp::NANOS_PER_DAY;
        int64 hours   = (ns / rpp::NANOS_PER_HOUR);   ns -= hours * rpp::NANOS_PER_HOUR;
        int64 minutes = (ns / rpp::NANOS_PER_MINUTE); ns -= minutes * rpp::NANOS_PER_MINUTE;
        int64 seconds = (ns / rpp::NANOS_PER_SEC);    ns -= seconds * rpp::NANOS_PER_SEC;
        int64 millis  = (ns / rpp::NANOS_PER_MILLI);

        len += snprintf(buf+len, bufsize-len, "%02lld:%02lld:%02lld.%03lldms",
                        hours, minutes, seconds, millis);
        return len;
    }
    int Duration::to_string(char* buf, int bufsize) const noexcept
    {
        return time_to_string(nanos(), buf, bufsize);
    }
    std::string Duration::to_string() const noexcept
    {
        char buf[128];
        int len = time_to_string(nanos(), buf, sizeof(buf));
        return {buf,buf+len};
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    TimePoint TimePoint::now() noexcept
    {
        #if _WIN32
            static int64 freq;
            if (!freq)
            {
                LARGE_INTEGER f; QueryPerformanceFrequency(&f);
                freq = int64(f.QuadPart);
            }

            LARGE_INTEGER time; QueryPerformanceCounter(&time);
            // convert ticks to nanoseconds
            // however, we need to be careful about overflow, which will always happen if we 
            // multiply by 1'000'000'000, so we need to split it into seconds and fractional seconds
            int64 seconds = time.QuadPart / freq;
            int64 fraction = time.QuadPart % freq;
            // convert all to nanoseconds
            return TimePoint{ (seconds*NANOS_PER_SEC) + ((fraction*NANOS_PER_SEC) / freq) };
        #else
            struct timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            return TimePoint{ int64(t.tv_sec * NANOS_PER_SEC + t.tv_nsec) };
        #endif
    }

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

    ScopedPerfTimer::ScopedPerfTimer(const char* prefix, const char* location, const char* detail) noexcept
        : prefix{prefix}
        , location{location}
        , detail{detail}
        , start{TimePoint::now()}
    {
    }

    ScopedPerfTimer::~ScopedPerfTimer() noexcept
    {
        // measure elapsed time as the first operation in the destructor
        TimePoint now = TimePoint::now();
        double elapsed_ms = start.elapsed_sec(now) * 1000.0;

        const char* padDetail = detail ? " " : "";
        detail = detail ? detail : "";
        prefix = prefix ? prefix : "";

        #if HAS_DEBUGGING_H
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

    void StopWatch::reset()noexcept
    {
        begin = {};
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

extern "C" double time_now_seconds()
{
    return rpp::TimePoint::now().duration.sec();
}

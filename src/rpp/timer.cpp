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
    // this should be the highest precision clock available on Windows
    // which is still synchronized with the system clock
    // 100ns ticks since 1601-01-01
    // https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
    static ULONGLONG win32_system_time() noexcept
    {
        FILETIME filetime;
        GetSystemTimePreciseAsFileTime(&filetime);
        ULARGE_INTEGER time;
        time.LowPart = filetime.dwLowDateTime;
        time.HighPart = filetime.dwHighDateTime;
        return time.QuadPart;
    }
    // uses multimedia timer API-s to sleep more accurately
    static void win32_sleep_ns(uint64 nanos)
    {
        if (nanos == 0)
        {
            // sleep 0 is special, gives priority to other threads, otherwise returns immediately
            SleepEx(0, /*alertable*/TRUE);
            return;
        }

        uint64 start = win32_system_time();
        MMRESULT timeBeginStatus = timeBeginPeriod(1); // set 1ms precision
        bool waited_enough = false;

        if (HANDLE hTimer = CreateWaitableTimer(NULL, TRUE, NULL))
        {
            int64 max_ticks = nanos / 100LL; // convert nanos to 100ns ticks
            LARGE_INTEGER dueTime; dueTime.QuadPart = -max_ticks;
            if (SetWaitableTimer(hTimer, &dueTime, 0, NULL, NULL, 0))
                WaitForSingleObject(hTimer, INFINITE);
            else
                waited_enough = false;
            CloseHandle(hTimer);
        }

        // calculate remaining time to sleep
        uint64 elapsed_ns = (win32_system_time() - start) * 100LL;
        if (elapsed_ns < nanos)
        {
            uint64 remaining_ns = nanos - elapsed_ns;
            int ms = static_cast<int>(remaining_ns / 1'000'000);
            if (ms <= 0) ms = 1;
            SleepEx(ms, /*alertable*/TRUE);
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

    static int duration_to_string(int64 ns, char* buf, int bufsize) noexcept
    {
        if (bufsize < 25)
            return 0; // won't fit

        char* end = buf;
        if (ns < 0)
        {
            ns = -ns;
            *end++ = '-';
        }

        if (ns >= NANOS_PER_YEAR)
        {
            int years = int(ns / NANOS_PER_YEAR);
            ns -= years * NANOS_PER_YEAR;

            *end++ = (years / 1000) + '0';
            *end++ = ((years / 100) % 10) + '0';
            *end++ = ((years / 10) % 10) + '0';
            *end++ = (years % 10) + '0';
            *end++ = 'y';
            *end++ = '-';
        }

        if (ns >= NANOS_PER_DAY)
        {
            int days = int(ns / NANOS_PER_DAY);
            ns -= days * NANOS_PER_DAY;

            *end++ = (days / 100) + '0';
            *end++ = ((days / 10) % 10) + '0';
            *end++ = (days % 10) + '0';
            *end++ = 'd';
            *end++ = ' ';
        }

        int hours = int(ns / NANOS_PER_HOUR);
        ns -= hours * NANOS_PER_HOUR;
        *end++ = (hours / 10) + '0';
        *end++ = (hours % 10) + '0';
        *end++ = ':';

        int minutes = int(ns / NANOS_PER_MINUTE);
        ns -= minutes * NANOS_PER_MINUTE;
        *end++ = (minutes / 10) + '0';
        *end++ = (minutes % 10) + '0';
        *end++ = ':';

        int seconds = int(ns / NANOS_PER_SEC);
        ns -= seconds * NANOS_PER_SEC;
        *end++ = (seconds / 10) + '0';
        *end++ = (seconds % 10) + '0';
        *end++ = '.';

        int millis = int(ns / NANOS_PER_MILLI);
        *end++ = ((millis / 100) % 10) + '0';
        *end++ = ((millis / 10) % 10) + '0';
        *end++ = (millis % 10) + '0';
        *end = '\0';
        return int(end - buf);
    }

    int Duration::to_string(char* buf, int bufsize) const noexcept
    {
        return duration_to_string(nanos(), buf, bufsize);
    }

    std::string Duration::to_string() const noexcept
    {
        char buf[64];
        int len = duration_to_string(nanos(), buf, sizeof(buf));
        return {buf, buf+len};
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    // YYYY-MM-DD HH:MM:SS.mmm
    static int datetime_to_string(int64 ns, char* buf, int bufsize) noexcept
    {
        if (bufsize < 28)
            return 0; // won't fit

        // for datetime we must use the same OS functions that we use for TimePoint::now()
        // in order to get the accurate system time as a string
    #if _WIN32
        // for windows we used GetSystemTimePreciseAsFileTime(&filetime);
        // which gives us the current time in 100ns ticks since 1601-01-01
        // we can convert this to a string using FileTimeToSystemTime
        ULARGE_INTEGER time; time.QuadPart = ns / 100LL;
        FILETIME time_as_filetime;
        time_as_filetime.dwLowDateTime = time.LowPart;
        time_as_filetime.dwHighDateTime = time.HighPart;
        SYSTEMTIME utc_time;
        if (!FileTimeToSystemTime(&time_as_filetime, &utc_time))
            return 0; // conversion failed

        // now format to YYYY-MM-DD HH:MM:SS.mmm
        char* end = buf;
        // YYYY-MM-DD
        for (int year = utc_time.wYear; year > 0; year /= 10)
            *end++ = (year % 10) + '0';
        *end++ = '-';
        *end++ = (utc_time.wMonth / 10) + '0';
        *end++ = (utc_time.wMonth % 10) + '0';
        *end++ = '-';
        *end++ = (utc_time.wDay / 10) + '0';
        *end++ = (utc_time.wDay % 10) + '0';
        *end++ = ' ';
        // HH:MM:SS.mmm
        *end++ = (utc_time.wHour / 10) + '0';
        *end++ = (utc_time.wHour % 10) + '0';
        *end++ = ':';
        *end++ = (utc_time.wMinute / 10) + '0';
        *end++ = (utc_time.wMinute % 10) + '0';
        *end++ = ':';
        *end++ = (utc_time.wSecond) + '0';
        *end++ = (utc_time.wSecond) + '0';
        *end++ = '.';
        *end++ = ((utc_time.wMilliseconds / 100) % 10) + '0';
        *end++ = ((utc_time.wMilliseconds / 10) % 10) + '0';
        *end++ = (utc_time.wMilliseconds % 10) + '0';
        *end = '\0';
        return int(end - buf);
    #else
        // for linux we used clock_gettime(CLOCK_REALTIME, &t);
        // which gives us the current time in seconds and nanoseconds
        // we can convert this to a string using the standard C functions
        time_t seconds = ns / NANOS_PER_SEC;
        int64 nanos = ns % NANOS_PER_SEC;
        struct tm* utc_time = gmtime(&seconds);

        // Format the date and time in the buffer
        char* end = buf + strftime(buf, bufsize, "%Y-%m-%d %H:%M:%S", utc_time);

        // append the milliseconds
        int millis = nanos / 1'000'000;
        *end++ = ((millis / 100) % 10) + '0';
        *end++ = ((millis / 10) % 10) + '0';
        *end++ = (millis % 10) + '0';
        *end = '\0';
        return int(end - buf);
    #endif
    }

    int TimePoint::to_string(char* buf, int bufsize) const noexcept
    {
        return duration_to_string(duration.nanos(), buf, bufsize);
    }

    std::string TimePoint::to_string() const noexcept
    {
        char buf[64];
        int len = duration_to_string(duration.nanos(), buf, sizeof(buf));
        return {buf, buf+len};
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    TimePoint TimePoint::now() noexcept
    {
        #if _WIN32
            // convert 100ns ticks to nanoseconds, this would overflow in 292 years
            return TimePoint{ int64(win32_system_time() * 100LL) };
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

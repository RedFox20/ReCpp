#include "timer.h"

#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <timeapi.h>
    #pragma comment(lib, "Winmm.lib") // MM time library
#elif __APPLE__ || __linux__ || __ANDROID__
    #include <stdio.h>  // fprintf
    #include <unistd.h> // usleep()
    #if __APPLE__
        #include <mach/mach.h>
        #include <mach/mach_time.h>
    #elif __linux__ || __ANDROID__ 
        #include <sys/time.h>
        #if __ANDROID__
            #include <android/log.h>
        #endif
    #endif
    #include <thread>
#elif __EMSCRIPTEN__
    #include <unistd.h> // usleep()
    #include <emscripten.h>
    #include <thread>
    #include <chrono>
    using namespace std::chrono;
#else
    #include <chrono>
    using namespace std::chrono;
#endif

#if defined(__has_include) && __has_include("debugging.h")
    #define HAS_DEBUGGING_H 1
    #include "debugging.h"
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////////

    static constexpr long NANOS_PER_SECOND = 1'000'000'000L;

    struct Period
    {
        // clock ticks divided by this period_den gives duration in seconds
        // seconds multiplied by this gives clock ticks
        double period_den;

        // clock ticks multiplied by this period gives duration in seconds
        // seconds divided by this gives clock ticks
        double period_sec;
        double period_ms;
        double period_us;
        double period_ns;

        constexpr Period(double period_den, double period) noexcept
            : period_den{period_den}
            , period_sec{period}
            , period_ms{period * 1'000}
            , period_us{period * 1'000'000}
            , period_ns{period * NANOS_PER_SECOND}
        {
        }
    };

    #if _WIN32
        static const Period period = []() -> Period
        {
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            double period_den = double(freq.QuadPart);
            return { period_den, 1.0 / period_den };
        }();
    #elif __APPLE__
        static const Period period = []() -> Period
        {
            mach_timebase_info_data_t timebase;
            mach_timebase_info(&timebase);
            double period = (double(timebase.numer) / timebase.denom / 1e9);
            return { 1.0 / period, period };
        }();
    #elif __linux__
        static constexpr Period period = { NANOS_PER_SECOND, 1.0 / double(NANOS_PER_SECOND) };
    #elif __EMSCRIPTEN__
        static constexpr Period period = { 1'000'000, 1.0 / 1'000'000.0 };
    #else
        static const Period period = []() -> Period
        {
            double period = double(microseconds::period::num) / microseconds::period::den;
            return { 1.0 / period, period };
        }();
    #endif

    double time_period() noexcept { return period.period_sec; }

    // time now in ticks (not as fast as accurate as TimePoint::now())
    uint64_t time_now() noexcept
    {
        #if _WIN32
            LARGE_INTEGER time;
            QueryPerformanceCounter(&time);
            return time.QuadPart;
        #elif __APPLE__
            return mach_absolute_time();
        #elif __linux__
            struct timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            return (t.tv_sec * 1'000'000'000ull) + t.tv_nsec;
        #elif __EMSCRIPTEN__
            return uint64_t(emscripten_get_now() * 1000);
        #else
            return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        #endif
    }

    int64_t from_sec_to_time_ticks(double seconds) noexcept
    {
        return int64_t(seconds * period.period_den);
    }
    int64_t from_ms_to_time_ticks(double millis) noexcept
    {
        return int64_t(millis / 1000.0 * period.period_den);
    }
    int64_t from_us_to_time_ticks(double micros) noexcept
    {
        return int64_t(micros / 1'000'000.0 * period.period_den);
    }
    int64_t from_ns_to_time_ticks(double nanos) noexcept
    {
        return int64_t(nanos / 1'000'000'000.0 * period.period_den);
    }

    double time_ticks_to_sec(int64_t ticks) noexcept
    {
        return ticks * period.period_sec;
    }
    double time_ticks_to_ms(int64_t ticks) noexcept
    {
        return ticks * period.period_ms;
    }
    double time_ticks_to_us(int64_t ticks) noexcept
    {
        return ticks * period.period_us;
    }
    double time_ticks_to_ns(int64_t ticks) noexcept
    {
        return ticks * period.period_ns;
    }

#if _WIN32
    static void periodSleep(MMRESULT& status, DWORD milliseconds)
    {
        if (status == -1)
        {
            status = timeBeginPeriod(1); // set 1ms precision
        }
        constexpr int64_t mmTicksToMillis = 10000; // 1 tick = 100nsec
        LARGE_INTEGER dueTime;
        dueTime.QuadPart = int64_t(milliseconds) * -mmTicksToMillis;

        if (HANDLE hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
            SetWaitableTimer(hTimer, &dueTime, 0, NULL, NULL, 0))
        {
            WaitForSingleObject(hTimer, INFINITE);
            CloseHandle(hTimer);
        }
        else
        {
            SleepEx(milliseconds, /*alertable*/TRUE);
        }
    }
    // win32 Sleep can be extremely inaccurate because the OS scheduler accuracy is 15.6ms by default
    // to work around that issue, most of the sleep can be done in one big sleep, and the remainder
    // can be spin looped with a Sleep(0) for yielding
    static void win32_sleep_ns(uint64_t nanos)
    {
        if (nanos == 0)
        {
            // sleep 0 is special, gives priority to other threads, otherwise returns immediately
            SleepEx(0, /*alertable*/TRUE);
            return;
        }

        // we need to measure exact suspend start time in case we timed out
        // because the suspension timeout is not accurate at all
        int64_t startTime = time_now();

        // for long waits, we first do a long suspended sleep, minus the windows timer tick rate
        static const int64_t millisecondTicks = from_ms_to_time_ticks(1.0);
        static const int64_t suspendThreshold = millisecondTicks * 16; // ms
        static const int64_t periodThreshold = millisecondTicks * 2; // ms

        MMRESULT timeBeginStatus = -1;

        // convert nanos to time_now() resolution:
        int64_t endTime = startTime + from_ns_to_time_ticks(double(nanos));
        int64_t remaining = endTime - time_now();
        while (remaining > 0)
        {
            // suspended sleep, it's quite difficult to get this accurate on Windows compared to other OS's
            if (remaining > suspendThreshold)
            {
                periodSleep(timeBeginStatus, 5);
            }
            else if (remaining > periodThreshold)
            {
                periodSleep(timeBeginStatus, 1/*ms*/);
            }
            else // spin loop is required because timeout is too short
            {
                // on low-cpu count systems, SleepEx can lock us out for an indefinite period,
                // so we're using yield for precision sleep here
                YieldProcessor();
            }

            remaining = endTime - time_now();
        }

        if (timeBeginStatus == TIMERR_NOERROR)
        {
            timeEndPeriod(1);
        }
    }
#elif __APPLE__ || __linux__ || __EMSCRIPTEN__
    inline void unix_sleep_ns_abstime(uint64_t nanos)
    {
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec  += (nanos / NANOS_PER_SECOND);
        deadline.tv_nsec += (nanos % NANOS_PER_SECOND);
        // normalize tv_nsec by overflowing into tv_sec
        if (deadline.tv_nsec >= NANOS_PER_SECOND) {
            deadline.tv_nsec -= NANOS_PER_SECOND;
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

    void sleep_ns(uint64_t nanos) noexcept
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

    Duration Duration::from_seconds(double seconds) noexcept
    {
        #if _WIN32 || __APPLE__
            return Duration{ int64_t(seconds * period.period_den) };
        #else
            seconds_t s = seconds_t(seconds);
            nseconds_t ns = nseconds_t((seconds - s) * NANOS_PER_SECOND);
            return Duration{ s, ns };
        #endif
    }
    Duration Duration::from_millis(int32_t millis) noexcept
    {
        #if _WIN32 || __APPLE__
            return Duration{ int64_t(millis / (1000.0 * period.period_den)) };
        #else
            return Duration{
                seconds_t(millis / 1000),
                nseconds_t((millis % 1000) * 1'000'000)
            };
        #endif
    }
    Duration Duration::from_micros(int32_t micros) noexcept
    {
        #if _WIN32 || __APPLE__
            return Duration{ int64_t(micros / (1'000'000.0 * period.period_den)) };
        #else
            return Duration{
                seconds_t(micros / 1'000'000),
                nseconds_t((micros % 1'000'000) * 1'000) };
        #endif
    }
    Duration Duration::from_nanos(int64_t nanos) noexcept
    {
        #if _WIN32 || __APPLE__
            return Duration{ int64_t(nanos / (1'000'000'000.0 * period.period_den)) };
        #else
            return Duration{ 
                seconds_t(nanos / NANOS_PER_SECOND), 
                nseconds_t(nanos % NANOS_PER_SECOND)
            };
        #endif
    }

    Duration Duration::operator+(const Duration& d) const noexcept
    {
        #if _WIN32 || __APPLE__
            return { ticks + d.ticks };
        #else
            auto s = sec + d.sec;
            auto ns = nsec + d.nsec; // (!) can be negative
            if (ns >= NANOS_PER_SECOND) {
                ns -= NANOS_PER_SECOND;
                ++s;
            } else if (ns < 0) {
                ns += NANOS_PER_SECOND;
                --s;
            }
            return { .sec = s, .nsec = ns };
        #endif
    }
    Duration Duration::operator-(const Duration& d) const noexcept
    {
        #if _WIN32 || __APPLE__
            return { ticks - d.ticks };
        #else
            auto s = sec - d.sec;
            auto ns = nsec - d.nsec; // (!) can be negative
            if (ns >= NANOS_PER_SECOND) {
                ns -= NANOS_PER_SECOND;
                ++s;
            } else if (ns < 0) {
                ns += NANOS_PER_SECOND;
                --s;
            }
            return { .sec = s, .nsec = ns };
        #endif
    }
    Duration& Duration::operator+=(const Duration& d) noexcept
    {
        #if _WIN32 || __APPLE__
            ticks += d.ticks;
        #else
            sec += d.sec;
            nsec += d.nsec; // (!) can be negative
            if (nsec >= NANOS_PER_SECOND) {
                nsec -= NANOS_PER_SECOND;
                ++sec;
            } else if (nsec < 0) {
                nsec += NANOS_PER_SECOND;
                --sec;
            }
        #endif
        return *this;
    }
    Duration& Duration::operator-=(const Duration& d) noexcept
    {
        #if _WIN32 || __APPLE__
            ticks -= d.ticks;
        #else
            sec -= d.sec;
            nsec -= d.nsec; // (!) can be negative
            if (nsec >= NANOS_PER_SECOND) {
                nsec -= NANOS_PER_SECOND;
                ++sec;
            } else if (nsec < 0) {
                nsec += NANOS_PER_SECOND;
                --sec;
            }
        #endif
        return *this;
    }

    double Duration::seconds() const noexcept
    {
        #if _WIN32 || __APPLE__
            return ticks * period.period_sec;
        #else
            return sec + (nsec * period.period_sec);
        #endif
    }
    int32_t Duration::millis() const noexcept
    {
        #if _WIN32 || __APPLE__
            return int32_t(ticks * period.period_ms);
        #else
            return int32_t((sec*1'000) + (nsec / 1'000'000));
        #endif
    }
    int32_t Duration::micros() const noexcept
    {
        #if _WIN32 || __APPLE__
            return int32_t(ticks * period.period_us);
        #else
            return int32_t((sec*1'000'000) + (nsec / 1'000));
        #endif
    }
    int64_t Duration::nanos() const noexcept
    {
        #if _WIN32 || __APPLE__
            return int64_t(ticks * period.period_ns);
        #else
            return int64_t((sec*1'000'000'000LL) + nsec);
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    TimePoint TimePoint::now() noexcept
    {
        #if _WIN32
            LARGE_INTEGER time;
            QueryPerformanceCounter(&time);
            return TimePoint{{ .ticks = int64_t(time.QuadPart) }};
        #elif __APPLE__
            return TimePoint{{ .ticks = int64_t(mach_absolute_time()) }};
        #else
            static_assert(sizeof(struct timespec) == sizeof(TimePoint), "TimePoint size mismatch");
            struct timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            // assign them explicitly to catch any errors in platform configuration
            return TimePoint{{ .sec = t.tv_sec, .nsec = t.tv_nsec }};
        #endif
    }

    double TimePoint::elapsed_sec(const TimePoint& end) const noexcept
    {
        return (end.dur - this->dur).seconds();
    }

    int32_t TimePoint::elapsed_ms(const TimePoint& end) const noexcept
    {
        return (end.dur - this->dur).millis();
    }

    int32_t TimePoint::elapsed_us(const TimePoint& end) const noexcept
    {
        return (end.dur - this->dur).micros();
    }

    int64_t TimePoint::elapsed_ns(const TimePoint& end) const noexcept
    {
        return (end.dur - this->dur).nanos();
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

    void StopWatch::start()
    {
        if (!begin) {
            begin = TimePoint::now();
            end = {};
        }
    }

    void StopWatch::stop()
    {
        if (begin && !end)
            end = TimePoint::now();
    }

    void StopWatch::resume()
    {
        end = {};
    }

    void StopWatch::reset()
    {
        begin = {};
        end = {};
    }

    double StopWatch::elapsed() const
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
    return rpp::time_now() * rpp::period.period_sec;
}

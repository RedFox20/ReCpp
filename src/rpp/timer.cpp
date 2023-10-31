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
        TimePoint start = TimePoint::now();

        // for long waits, we first do a long suspended sleep, minus the windows timer tick rate
        static const Duration suspendThreshold = Duration::from_millis(16);
        static const Duration periodThreshold = Duration::from_millis(2);

        MMRESULT timeBeginStatus = -1;

        // convert nanos to time_now() resolution:
        TimePoint endTime = start + Duration::from_nanos(nanos);;
        Duration remaining = (endTime - TimePoint::now());
        while (remaining > Duration::zero())
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

            remaining = endTime - TimePoint::now();
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

    TimePoint TimePoint::now() noexcept
    {
        #if _WIN32
            static uint64_t freq;
            if (!freq)
            {
                LARGE_INTEGER f; QueryPerformanceFrequency(&f);
                freq = uint64_t(f.QuadPart);
            }

            LARGE_INTEGER time; QueryPerformanceCounter(&time);

            // in order to support nanosecond precision,
            // the internal ticks need to be converted to nanoseconds
            auto sec = sec_t(time.QuadPart / freq);
            auto fraction = uint64_t(time.QuadPart % freq);
            // convert fraction to nanoseconds
            auto nsec = nsec_t((fraction * NANOS_PER_SEC) / freq);

            return TimePoint{{ sec, nsec }};
        #else
            struct timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            return TimePoint{ int64_t(t.tv_sec * NANOS_PER_SEC + t.tv_nsec) };
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
    return rpp::TimePoint::now().duration.seconds();
}

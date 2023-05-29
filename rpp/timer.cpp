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

    // clock ticks divided by this period_den gives duration in seconds
    // seconds multiplied by this gives clock ticks
    static const double period_den = []() -> double
    {
        #if _WIN32
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            return double(freq.QuadPart);
        #elif __APPLE__
            mach_timebase_info_data_t timebase;
            mach_timebase_info(&timebase);
            return (1.0 / (double(timebase.numer) / timebase.denom / 1e9));
        #elif __linux__
            return 1'000'000'000; // nanoseconds for clock_gettime
            // return 1'000'000; // microseconds for gettimeofday
        #elif __EMSCRIPTEN__
            return 1'000'000;
        #else // default to std::chrono
            return (1.0 / (double(microseconds::period::num) / microseconds::period::den));
        #endif
    } ();

    // clock ticks multiplied by this period gives duration in seconds
    // seconds divided by this gives clock ticks
    static const double period = []{ return 1.0 / period_den; }();

    double time_period() noexcept { return period; }

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
            // gettimeofday is apparently obsolete
            // struct timeval t;
            // gettimeofday(&t, NULL);
            // return (t.tv_sec * 1000000ull) + t.tv_usec;
        #elif __EMSCRIPTEN__
            return uint64_t(emscripten_get_now() * 1000);
        #else
            return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        #endif
    }

    int64_t from_sec_to_time_ticks(double seconds) noexcept
    {
        return int64_t(seconds * period_den);
    }
    int64_t from_ms_to_time_ticks(double millis) noexcept
    {
        return int64_t(millis / 1000.0 * period_den);
    }
    int64_t from_us_to_time_ticks(double micros) noexcept
    {
        return int64_t(micros / 1'000'000.0 * period_den);
    }

    int64_t ticks_to_millis(int64_t ticks) noexcept
    {
        double seconds = ticks * period;
        return int64_t(seconds * 1000);
    }

    double time_ticks_to_sec(int64_t ticks) noexcept
    {
        return ticks * period;
    }
    double time_ticks_to_ms(int64_t ticks) noexcept
    {
        return ticks * period * 1'000;
    }
    double time_ticks_to_us(int64_t ticks) noexcept
    {
        return ticks * period * 1'000'000;
    }

#if _WIN32
    void periodSleep(MMRESULT& status, DWORD milliseconds)
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
    void win32_sleep_us(uint64_t micros)
    {
        if (micros == 0)
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

        // convert micros to time_now() resolution:
        int64_t endTime = startTime + from_us_to_time_ticks(double(micros));
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
#endif

    // #if __APPLE__ || __linux__ || __EMSCRIPTEN__
    // void unix_sleep_us_abstime(uint64_t micros)
    // {
    //     struct timespec deadline;
    //     clock_gettime(CLOCK_MONOTONIC, &deadline);

    //     deadline.tv_sec  += (micros / 1'000'000ul);
    //     deadline.tv_nsec += (micros % 1'000'000ul) * 1'000ul;
    //     // normalize tv_nsec by overflowing into tv_sec
    //     if (deadline.tv_nsec >= 1000000000) {
    //         deadline.tv_nsec -= 1000000000;
    //         deadline.tv_sec++;
    //     }
    //     clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
    // }
    // #endif

    void sleep_ms(unsigned int millis) noexcept
    {
        #if _WIN32
            win32_sleep_us(millis * 1000);
        // #elif __APPLE__ || __linux__ || __EMSCRIPTEN__
        //     unix_sleep_us_abstime(millis * 1000);
        #else
            std::this_thread::sleep_for(std::chrono::milliseconds(millis));
        #endif
    }

    void sleep_us(unsigned int micros) noexcept
    {
        #if _WIN32
            win32_sleep_us(micros);
        // #elif __APPLE__ || __linux__ || __EMSCRIPTEN__
        //     unix_sleep_us_abstime(micros);
        #else
            std::this_thread::sleep_for(std::chrono::microseconds(micros));
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    Timer::Timer() noexcept
    {
        start();
    }
    
    Timer::Timer(StartMode startMode) noexcept
    {
        if (startMode == StartMode::AutoStart)
            start();
        else
            value = 0;
    }

    void Timer::start() noexcept
    {
        value = time_now();
    }

    double Timer::elapsed() const noexcept
    {
        uint64_t end = time_now();
        return (end - value) * rpp::period;
    }

    double Timer::next() noexcept
    {
        double t = elapsed();
        start();
        return t;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    ScopedPerfTimer::ScopedPerfTimer(const char* what) noexcept : what(what)
    {
    }

    ScopedPerfTimer::~ScopedPerfTimer() noexcept
    {
        double elapsed_ms = timer.elapsed_ms();
        #if HAS_DEBUGGING_H
            LogInfo("%s elapsed: %.3fms", what, elapsed_ms);
        #else
            constexpr const char format[] = "$ %s elapsed: %.3fms\n";
            #if _WIN32
                static HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
                SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN); // dark yellow
                fprintf(stderr, format, what, elapsed_ms);
                SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // default color
            #elif __ANDROID__
                __android_log_print(ANDROID_LOG_WARN, "rpp", format, what, elapsed_ms);
            #else // @todo Proper Linux & OSX implementations
                fprintf(stderr, format, what, elapsed_ms);
            #endif
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    void StopWatch::start()
    {
        if (!begin) {
            begin = time_now();
            end   = 0;
        }
    }

    void StopWatch::stop()
    {
        if (begin && !end)
            end = time_now();
    }

    void StopWatch::resume()
    {
        end = 0;
    }

    void StopWatch::reset()
    {
        begin = 0;
        end   = 0;
    }

    double StopWatch::elapsed() const
    {
        if (begin) {
            if (end) return (end - begin) * rpp::period;
            return (time_now() - begin) * rpp::period;
        }
        return 0.0;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

} // rpp

extern "C" double time_now_seconds()
{
    return rpp::time_now() * rpp::period;
}

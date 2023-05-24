#include "timer.h"

#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
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

    static const double period = []
    {
        #if _WIN32
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            return 1.0 / double(freq.QuadPart);
        #elif __APPLE__
            mach_timebase_info_data_t timebase;
            mach_timebase_info(&timebase);
            return double(timebase.numer) / timebase.denom / 1e9;
        #elif __linux__
            return 1e-9; // nanoseconds for clock_gettime
            // return 1e-6; // microseconds for gettimeofday
        #elif __EMSCRIPTEN__
            return 1e-6;
        #else // default to std::chrono
            return double(microseconds::period::num) / microseconds::period::den;
        #endif
    } ();

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

#if _WIN32
    // win32 Sleep can be extremely inaccurate because the OS scheduler accuracy is 15.6ms by default
    // to work around that issue, most of the sleep can be done in one big sleep, and the remainder
    // can be spin looped with a Sleep(0) for yielding
    void win32_sleep_us(uint64_t micros)
    {
        // for long waits, we first do a long suspended sleep, minus the windows timer tick rate
        constexpr uint64_t suspendThreshold = 16 * 1000; // 16ms as micros

        // convert micros to time_now() resolution:
        int64_t remaining = int64_t( (micros / 1'000'000.0) * period );
        if (remaining > suspendThreshold)
        {
            // we need to measure exact suspend start time in case we timed out
            // because the suspension timeout is not accurate at all
            int64_t suspendStart = time_now();

            int64_t timeout = remaining - suspendThreshold;
            if (timeout < 0) timeout = 0;
            Sleep(DWORD(timeout));

            int64_t now = time_now();
            remaining -= std::abs(now - suspendStart);
            if (remaining <= 0)
                return;
        }

        // spin loop is required because timeout is too short
        {
            int64_t prevTime = time_now();
            do
            {
                Sleep(0); // sleep 0 is special, gives priority to other threads

                int64_t now = time_now();
                remaining -= std::abs(now - prevTime);
                prevTime = now;
            }
            while (remaining > 0);
        }
    }
#endif

    void sleep_ms(unsigned int millis) noexcept
    {
        #if _WIN32
            win32_sleep_us(millis * 1000);
        #elif __APPLE__ || __linux__ || __EMSCRIPTEN__
            usleep(millis*1000);
        #else
            this_thread::sleep_for(milliseconds(millis));
        #endif
    }

    void sleep_us(unsigned int micros) noexcept
    {
        #if _WIN32
            win32_sleep_us(micros);
        #elif __APPLE__ || __linux__ || __EMSCRIPTEN__
            usleep(micros);
        #else
            this_thread::sleep_for(microseconds(micros)); // On Windows, this does Sleep(1) in a loop
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

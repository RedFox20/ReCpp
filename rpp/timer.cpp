#include "timer.h"

#if __APPLE__ || __linux__ || __ANDROID_API__
    #include <unistd.h> // usleep()
    #if __APPLE__
        #include <mach/mach.h>
        #include <mach/mach_time.h>
    #elif __linux__ || __ANDROID_API__ 
        #include <sys/time.h>
    #endif
#elif __EMSCRIPTEN__
    #include <unistd.h> // usleep()
    #include <emscripten.h>
#elif _WIN32 || _WIN64
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#else
    #include <thread>
    #include <chrono>
    using namespace std::chrono;
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////////

    static const double period = []
    {
        #if __APPLE__
            mach_timebase_info_data_t timebase;
            mach_timebase_info(&timebase);
            return double(timebase.numer) / timebase.denom / 1e9;
        #elif __linux__
            return 1e-6;
        #elif __EMSCRIPTEN__
            return 1e-6;
        #elif _WIN32
            LARGE_INTEGER freq;
            QueryPerformanceFrequency(&freq);
            return 1.0 / double(freq.QuadPart);
        #else // default to std::chrono
            return double(microseconds::period::num) / microseconds::period::den;
        #endif
    } ();

    double time_period() noexcept { return period; }

    uint64_t time_now() noexcept
    {
        #ifdef __APPLE__
            return mach_absolute_time();
        #elif __linux__
            struct timeval t;
            gettimeofday(&t, NULL);
            return (t.tv_sec * 1000000ull) + t.tv_usec;
        #elif __EMSCRIPTEN__
            return uint64_t(emscripten_get_now() * 1000);
        #elif _WIN32
            LARGE_INTEGER time;
            QueryPerformanceCounter(&time);
            return time.QuadPart;
        #else
            return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        #endif
    }

    void sleep_ms(unsigned int millis) noexcept
    {
        #if __APPLE__ || __linux__ || __EMSCRIPTEN__
            usleep(millis*1000);
        #elif _WIN32
            Sleep(millis);
        #else
            this_thread::sleep_for(milliseconds(millis));
        #endif
    }

    void sleep_us(unsigned int micros) noexcept
    {
        #if __APPLE__ || __linux__ || __EMSCRIPTEN__
            usleep(micros);
        #elif _WIN32
            // On Windows we don't really care much; just do a millisecond sleep
            int millis = micros / 1000;
            if (millis == 0) millis = 1;
            Sleep(millis);
        #else
            this_thread::sleep_for(microseconds(micros));
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    Timer::Timer()
    {
        start();
    }

    void Timer::start()
    {
        value = time_now();
    }

    double Timer::elapsed() const
    {
        uint64_t end = time_now();
        return (end - value) * period;
    }

    double Timer::next()
    {
        double t = elapsed();
        start();
        return t;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
}


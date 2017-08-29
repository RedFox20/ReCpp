#include "timer.h"

#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#elif __APPLE__ || __linux__ || __ANDROID_API__
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
    #include <thread>
    #include <chrono>
    using namespace std::chrono;
#endif
#include <chrono>
    using namespace std::chrono;
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
            return 1e-6;
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
            struct timeval t;
            gettimeofday(&t, NULL);
            return (t.tv_sec * 1000000ull) + t.tv_usec;
        #elif __EMSCRIPTEN__
            return uint64_t(emscripten_get_now() * 1000);
        #else
            return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
        #endif
    }

    void sleep_ms(unsigned int millis) noexcept
    {
        #if _WIN32
            Sleep(millis);
        #elif __APPLE__ || __linux__ || __EMSCRIPTEN__
            usleep(millis*1000);
        #else
            this_thread::sleep_for(milliseconds(millis));
        #endif
    }

    void sleep_us(unsigned int micros) noexcept
    {
        #if _WIN32
            if (micros >= 1000)
            {
                Sleep(micros / 1000); // this is not really accurate either... maybe always use NtDelayExecution?
            }
            else
            {
                // On Windows it's difficult to get sub-millisecond precision, so no guarantees...
                static auto NtDll = GetModuleHandleW(L"ntdll.dll");
                static auto NtDelayExecution     = (long(__stdcall*)(BOOL alertable, PLARGE_INTEGER delayInterval))GetProcAddress(NtDll, "NtDelayExecution");
                static auto ZwSetTimerResolution = (long(__stdcall*)(ULONG requestedRes, BOOL setNew, ULONG* actualRes))GetProcAddress(NtDll, "ZwSetTimerResolution");

                static bool resolutionSet;
                if (!resolutionSet) {
                    ULONG actualResolution;
                    ZwSetTimerResolution(1, true, &actualResolution);
                    resolutionSet = true;
                }

                LARGE_INTEGER interval;
                interval.QuadPart = micros * -10000000;
                NtDelayExecution(false, &interval);
            }
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

    void Timer::start() noexcept
    {
        value = time_now();
    }

    double Timer::elapsed() const noexcept
    {
        uint64_t end = time_now();
        return (end - value) * period;
    }

    double Timer::next() noexcept
    {
        double t = elapsed();
        start();
        return t;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
}


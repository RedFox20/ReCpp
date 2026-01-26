#include "timer.h"
#include <ctime> // gmtime, timegm
#include "strview.h"

#if _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <timeapi.h>
    #include <sysinfoapi.h> // GetSystemTimePreciseAsFileTime
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
#elif RPP_BARE_METAL
    #include <cstdint>
    #define timegm mktime
    #include <printf/printf.h>
    #include <cstdarg>

    #if RPP_FREERTOS
        #include <FreeRTOS.h>
        #include <task.h>
    #endif

    #if RPP_STM32_HAL
        #include RPP_STM32_HAL_H
    #endif

    #if RPP_CORTEX_M_ARCH
        #include RPP_CORTEX_M_CORE_H
    #endif
#endif

#if defined(__has_include) && __has_include("debugging.h")
    #define HAS_DEBUGGING_H 1
    #include "debugging.h"
#endif

#ifdef _WIN32
    #define timegm _mkgmtime
#endif

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////////

#if _WIN32
    static FINLINE uint64 to_uint64(const FILETIME& f) noexcept
    {
        ULARGE_INTEGER time;
        time.LowPart = f.dwLowDateTime;
        time.HighPart = f.dwHighDateTime;
        return time.QuadPart;
    }
    static FINLINE FILETIME to_filetime(uint64 ticks) noexcept
    {
        FILETIME filetime;
        ULARGE_INTEGER time; time.QuadPart = ticks;
        filetime.dwLowDateTime = time.LowPart;
        filetime.dwHighDateTime = time.HighPart;
        return filetime;
    }
    static FINLINE int64 ticks_to_ns(uint64 ticks) noexcept
    {
        return ticks * 100LL;
    }

    // this should be the highest precision clock available on Windows
    // which is still synchronized with the system clock
    // 100ns ticks since 1601-01-01
    // https://docs.microsoft.com/en-us/windows/win32/sysinfo/acquiring-high-resolution-time-stamps
    static FINLINE uint64 get_time_ticks() noexcept
    {
        FILETIME filetime;
        GetSystemTimePreciseAsFileTime(&filetime);
        return to_uint64(filetime);
    }

    static constexpr uint64 LINUX_EPOCH_TICKS = 116444736000000000ull; // 1970-01-01 - 1601-1-1

    /// @note Reused from other WINAPI modules
    static FINLINE uint64 get_ticks_since_epoch(FILETIME& filetime) noexcept
    {
        return to_uint64(filetime) - LINUX_EPOCH_TICKS;
    }

    // convert the high precision system time to a unix epoch time
    static uint64 ticks_since_epoch() noexcept
    {
        FILETIME filetime;
        GetSystemTimePreciseAsFileTime(&filetime);
        return get_ticks_since_epoch(filetime);
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

        uint64 start_ticks = ticks_since_epoch();
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
        uint64 elapsed_ns = ticks_to_ns(ticks_since_epoch() - start_ticks);
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
#elif RPP_BARE_METAL
    #if RPP_CORTEX_M_ARCH
        /**
         * Cortex-M platform alone does not have a timer. Timing
         * depends on a higher-level platform (e.g., FreeRTOS, STM32 HAL).
         */
        FINLINE static int get_tick() noexcept;
        FINLINE static int get_tick_rate_hz() noexcept;

        /** @brief Get time in nanoseconds. This depends on get_tick and get_tick_rate_hz. */
        static int64 cortex_m_get_time_ns() noexcept
        {
            int tick;
            int st;

            do
            {
                tick = get_tick();
                st = SysTick->VAL;
                // Clear the instruction pipeline
                asm volatile("nop");
                asm volatile("nop");
            } while (tick != get_tick()); // Ensure no rollover

            int ticks_per_second = get_tick_rate_hz();
            int ns_per_tick = 1'000'000'000 / ticks_per_second;
            int64 cycles_per_tick = SysTick->LOAD + 1;

            // Current tick count times ns_per_tick
            int64 tick_time = int64{tick} * ns_per_tick;
            // How far along we are into the next tick in nanoseconds
            // NOTE: SysTick counts down, therefore we use (cycles_per_tick - st)
            int64 tick_fraction = int64{cycles_per_tick - st} * ns_per_tick / cycles_per_tick;

            return tick_time + tick_fraction;
        }

        /** @brief Wait for a specified duration in nanoseconds. */
        static void cortex_m_sleep_ns(unsigned int duration) noexcept
        {
            int64 start = cortex_m_get_time_ns();
            int64 end = start + duration;

            if (end < start)
            {
                // Sleep until timer wraps around
                while (cortex_m_get_time_ns() > start) {}
            }

            while (cortex_m_get_time_ns() < end) {}
        }
    #endif

    #if RPP_FREERTOS
        FINLINE static int get_tick() noexcept { return xTaskGetTickCount(); }
        FINLINE static int get_tick_rate_hz() noexcept { return configTICK_RATE_HZ; }

        // Platform specific implementation of getting time in nanoseconds
        static int64 freertos_get_time_ns() noexcept
        {
        #if RPP_CORTEX_M_ARCH
            return cortex_m_get_time_ns();
        #else
            return 1'000'000'000ull * xTaskGetTickCount() / configTICK_RATE_HZ;
        #endif
        }

        static void freertos_sleep_ns(unsigned int duration) noexcept
        {
            int ticks = duration / configTICK_RATE_HZ;
            int nanos = duration - (ticks * configTICK_RATE_HZ);

            // Sleep for full ticks
            if (ticks > 0)
                vTaskDelay(ticks);

            // Spin-wait for remaining nanoseconds
            if (nanos > 0)
            {
                int64 start = freertos_get_time_ns();
                int64 end = start + nanos;

                if (end < start)
                {
                    // Sleep until timer wraps around
                    while (freertos_get_time_ns() > start) {}
                }

                while (freertos_get_time_ns() < end) {}
            }
        }

    #elif RPP_STM32_HAL
        /**
         * @note While it is called HAL_GetTick, it actually returns time in milliseconds
         * since system start (ticks * tick period in ms). This needs to be divided by
         * the tick period in ms to get the actual tick count.
         * @note While it is called HAL_GetTickFreq, it actually returns the tick period in
         * milliseconds, e.g, if the tick frequency is 1000Hz, it returns 1 (ms). Divide 1000ms
         * by this value to get the tick frequency in Hz.
         */
        FINLINE static int get_tick() noexcept {
            uint32_t current_ms = HAL_GetTick();
            uint32_t tick_period_in_ms = HAL_GetTickFreq();
            return current_ms / tick_period_in_ms;        
        }
        
        FINLINE static int get_tick_rate_hz() noexcept {
            uint32_t tick_period_in_ms = HAL_GetTickFreq();
            return 1000 / tick_period_in_ms;
        }
    #endif
#endif

    void sleep_ms(unsigned int millis) noexcept
    {
        #if _WIN32
            win32_sleep_ns(millis * 1'000'000ull);
        #elif __APPLE__ || __linux__ || __EMSCRIPTEN__
            unix_sleep_ns_abstime(millis * 1'000'000ull);
        #elif RPP_FREERTOS
            freertos_sleep_ns(millis * 1'000'000ull);
        #elif RPP_STM32_HAL
            cortex_m_sleep_ns(millis * 1'000'000ull);
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
        #elif RPP_FREERTOS
            freertos_sleep_ns(micros * 1'000ull);
        #elif RPP_STM32_HAL
            cortex_m_sleep_ns(micros * 1'000ull);
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
        #elif RPP_FREERTOS
            freertos_sleep_ns(nanos);
        #elif RPP_STM32_HAL
            cortex_m_sleep_ns(nanos);
        #else
            std::this_thread::sleep_for(std::chrono::nanoseconds{nanos});
        #endif
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////

    NOINLINE static int print_fraction(int64 ns, char* out, int fraction_digits) noexcept
    {
        if      (fraction_digits < 1) fraction_digits = 1;
        else if (fraction_digits > 9) fraction_digits = 9;
        int number = 0;
        switch (fraction_digits)
        {
            case 1: number = int(ns / 100'000'000); break;
            case 2: number = int(ns / 10'000'000); break;
            case 3: number = int(ns / 1'000'000); break;
            case 4: number = int(ns / 100'000); break;
            case 5: number = int(ns / 10'000); break;
            case 6: number = int(ns / 1'000); break;
            case 7: number = int(ns / 100); break;
            case 8: number = int(ns / 10); break;
            case 9: number = int(ns); break;
        }
        char buf[16];
        int len = rpp::to_string(buf, number);
        // now write out the result
        *out++ = '.';
        for (int i = 0, pad = fraction_digits - len; i < pad; ++i) // pad with zeroes
            *out++ = '0';
        for (int i = 0; i < len; ++i)
            *out++ = buf[i];
        return fraction_digits + 1;
    }

    inline static int print_3digits(int value, char* out) noexcept
    {
        *out++ = (value / 100) + '0';
        *out++ = ((value / 10) % 10) + '0';
        *out++ = (value % 10) + '0';
        return 3;
    }

    inline static int print_2digits(int value, char* out) noexcept
    {
        *out++ = (value / 10) + '0';
        *out++ = (value % 10) + '0';
        return 2;
    }

    inline static int print_2digits(int value, char* out, char sep) noexcept
    {
        *out++ = (value / 10) + '0';
        *out++ = (value % 10) + '0';
        *out++ = sep;
        return 3;
    }

    NOINLINE static int print_digits(int value, char* out, char sep) noexcept
    {
        int len = 0;
        value = abs(value);
        do {
            out[len++] = (value % 10) + '0';
            value /= 10;
        } while (value > 0);

        // reverse the string
        char* ptr = out;
        char* end = out + len;
        while (ptr < end) { char tmp = *ptr; *ptr++ = *--end; *end = tmp; }
        out[len++] = sep;
        return len;
    }

    NOINLINE static int duration_to_string(int64 ns, char* buf, int bufsize, int fraction_digits) noexcept
    {
        if (bufsize < 25)
            return 0; // won't fit
        char* end = buf;

        if (ns < 0) {
            // handle edge case, INT64_MIN is not representable as positive
            // -9223372036854775808 --> 9223372036854775807
            ns = (ns == INT64_MIN) ? INT64_MAX : -ns;
            *end++ = '-';
        }

        if (ns >= NANOS_PER_YEAR) {
            int years = int(ns / NANOS_PER_YEAR);
            ns -= years * NANOS_PER_YEAR;
            end += print_digits(years, end, '-');
        }
        if (ns >= NANOS_PER_DAY) {
            int days = int(ns / NANOS_PER_DAY);
            ns -= days * NANOS_PER_DAY;
            end += print_digits(days, end, '-');
        }
        int hours = int(ns / NANOS_PER_HOUR);
        ns -= hours * NANOS_PER_HOUR;
        end += print_2digits(hours, end, ':');
        int minutes = int(ns / NANOS_PER_MINUTE);
        ns -= minutes * NANOS_PER_MINUTE;
        end += print_2digits(minutes, end, ':');
        int seconds = int(ns / NANOS_PER_SEC);
        ns -= seconds * NANOS_PER_SEC;
        end += print_2digits(seconds, end);

        if (fraction_digits > 0)
            end += print_fraction(ns, end, fraction_digits);

        *end = '\0';
        return int(end - buf);
    }

    int Duration::to_string(char* buf, int bufsize, int fraction_digits) const noexcept
    {
        return duration_to_string(nanos(), buf, bufsize, fraction_digits);
    }

#if !RPP_BARE_METAL
    std::string Duration::to_string(int fraction_digits) const noexcept
    {
        char buf[64];
        int len = duration_to_string(nanos(), buf, sizeof(buf), fraction_digits);
        return {buf, buf+len};
    }
#endif

    NOINLINE static int duration_to_stopwatch_string(int64 ns, char* buf, int bufsize, int fraction_digits) noexcept
    {
        // int64 has max 153,722,867 minutes which requires 9 chars
        // "[(-)[9]m [2]s [3]ms [3]us [3]ns]"
        // 1 + (1) + 11 + 4 + 6 + 6 + 5 + 1 = 36 chars
        if (bufsize < 36)
            return 0; // won't fit

        char* end = buf;
        *end++ = '[';

        if (ns < 0) {
            // handle edge case, INT64_MIN is not representable as positive
            // -9223372036854775808 --> 9223372036854775807
            ns = (ns == INT64_MIN) ? INT64_MAX : -ns;
            *end++ = '-';
        }

        if (ns >= NANOS_PER_MINUTE)
        {
            int64 minutes = int64(ns / NANOS_PER_MINUTE);
            ns -= minutes * NANOS_PER_MINUTE;
            end += print_digits(static_cast<int>(minutes), end, 'm');
            *end++ = ' ';
        }

        // always display seconds, so if we get `ns=0` we still print "0s"
        // max 9'223'372'036 seconds which requires 
        int seconds = int(ns / NANOS_PER_SEC);
        end += print_digits(seconds, end, 's');

        // TODO: currently fraction printing is simplified to 3 digit versions only
        if (fraction_digits > 0)
        {
            *end++ = ' ';
            int frac_ns = static_cast<int>(ns - seconds * NANOS_PER_SEC);
            int frac_ms = frac_ns / NANOS_PER_MILLI;

            if (fraction_digits >= 1)
            {
                end += print_3digits(frac_ms, end);
                *end++ = 'm';
                *end++ = 's';
            }
            if (fraction_digits >= 4)
            {
                *end++ = ' ';
                int frac_us = frac_ns / NANOS_PER_MICRO - (frac_ms * 1000);
                end += print_3digits(frac_us, end);
                *end++ = 'u';
                *end++ = 's';
            }
            if (fraction_digits >= 7)
            {
                *end++ = ' ';
                end += print_3digits(frac_ns % 1000, end);
                *end++ = 'n';
                *end++ = 's';
            }
        }

        *end++ = ']';
        *end = '\0';
        return int(end - buf);
    }

    int Duration::to_stopwatch_string(char* buf, int bufsize, int fraction_digits) const noexcept
    {
        return duration_to_stopwatch_string(nanos(), buf, bufsize, fraction_digits);
    }

#if !RPP_BARE_METAL
    std::string Duration::to_stopwatch_string(int fraction_digits) const noexcept
    {
        char buf[64];
        int len = duration_to_stopwatch_string(nanos(), buf, sizeof(buf), fraction_digits);
        return {buf, buf+len};
    }
#endif

    ///////////////////////////////////////////////////////////////////////////////////////////////

    static std::tm gmtime_safe(const time_t& time) noexcept
    {
        std::tm time_tm;
        #if _MSC_VER // MSVC++
            gmtime_s(&time_tm, &time); // arguments reversed for some reason
        #else
            gmtime_r(&time, &time_tm);
        #endif
        return time_tm;
    }

    static std::tm localtime_safe(const time_t& time) noexcept
    {
        std::tm time_tm;
        #if _MSC_VER // MSVC++
            localtime_s(&time_tm, &time); // arguments reversed for some reason
        #else
            localtime_r(&time, &time_tm);
        #endif
        return time_tm;
    }

    // YYYY-MM-DD HH:MM:SS.mmm
    NOINLINE static int datetime_to_string(int64 ns, char* buf, int bufsize, int fraction_digits) noexcept
    {
        if (bufsize < 28)
            return 0; // won't fit

        // for datetime we must use the same OS functions that we use for TimePoint::now()
        // in order to get the accurate system time as a string

    #if _MSC_VER
        // for windows we used GetSystemTimePreciseAsFileTime(&filetime);
        // which gives us the current time in 100ns ticks since 1601-01-01
        // we can convert this to a string using FileTimeToSystemTime
        // convert nanoseconds to 100ns ticks
        int64 systime_ticks = LINUX_EPOCH_TICKS + (ns / 100LL);
        FILETIME filetime = to_filetime(systime_ticks);
        SYSTEMTIME utc_time;
        if (FileTimeToSystemTime(&filetime, &utc_time))
        {
            // now format to YYYY-MM-DD HH:MM:SS.mmm
            char* end = buf;
            end += print_digits(utc_time.wYear, end, '-');
            end += print_2digits(utc_time.wMonth, end, '-');
            end += print_2digits(utc_time.wDay, end, ' ');
            end += print_2digits(utc_time.wHour, end, ':');
            end += print_2digits(utc_time.wMinute, end, ':');
            end += print_2digits(utc_time.wSecond, end);
            if (fraction_digits > 0)
                end += print_fraction(ns % NANOS_PER_SEC, end, fraction_digits);
            *end = '\0';
            return int(end - buf);
        }
    #endif

        // for linux we used clock_gettime(CLOCK_REALTIME, &t);
        // which gives us the current time in seconds and nanoseconds
        // we can convert this to a string using the standard C functions
        time_t seconds = ns / NANOS_PER_SEC;
        int64 nanos = ns % NANOS_PER_SEC;
        struct tm tm_utc = gmtime_safe(seconds);

        // Format the date and time in the buffer
    #if RPP_BARE_METAL
        // Use snprintf since strftime footprint is very large (~7KB)
        char* end = buf + snprintf_(buf, bufsize, "%04d-%02d-%02d %02d:%02d:%02d",
            tm_utc.tm_year + 1900,
            tm_utc.tm_mon + 1,
            tm_utc.tm_mday,
            tm_utc.tm_hour,
            tm_utc.tm_min,
            tm_utc.tm_sec);
    #else
        char* end = buf + strftime(buf, bufsize, "%Y-%m-%d %H:%M:%S", &tm_utc);
    #endif
        if (fraction_digits > 0)
            end += print_fraction(nanos, end, fraction_digits);
        *end = '\0';
        return int(end - buf);
    }

    TimePoint::TimePoint(int year, int month, int day, int hour, int minute, int second, int64 nanos) noexcept
    {
        #if _WIN32
            // use system time
            SYSTEMTIME systime;
            systime.wYear = year;
            systime.wMonth = month;
            systime.wDay = day;
            systime.wHour = hour;
            systime.wMinute = minute;
            systime.wSecond = second;
            systime.wMilliseconds = 0;

            FILETIME filetime;
            if (SystemTimeToFileTime(&systime, &filetime))
            {
                uint64 ticks_from_epoch = get_ticks_since_epoch(filetime);
                duration = Duration{ticks_to_ns(ticks_from_epoch) + nanos};
                return;
            }
        #endif

        std::tm tm {};
        tm.tm_year = year - 1900; // [0, 60] since 1900
        tm.tm_mon  = month - 1;   // [0, 11] since Jan
        tm.tm_mday = day;         // [1, 31]
        tm.tm_hour = hour;        // [0, 23] since midnight
        tm.tm_min  = minute;      // [0, 59] after the hour
        tm.tm_sec  = second;      // [0, 60] after the min allows for 1 positive leap second
        tm.tm_isdst = 0;          // [-1...] -1 for unknown, 0 for not DST, any positive value if DST.

        time_t seconds = timegm(&tm); // input time as-is
        duration = Duration{int64(seconds * NANOS_PER_SEC + nanos)};
    }

    int TimePoint::to_string(char* buf, int bufsize, int fraction_digits) const noexcept
    {
        return datetime_to_string(duration.nanos(), buf, bufsize, fraction_digits);
    }

#if !RPP_BARE_METAL
    std::string TimePoint::to_string(int fraction_digits) const noexcept
    {
        char buf[64];
        int len = datetime_to_string(duration.nanos(), buf, sizeof(buf), fraction_digits);
        return {buf, buf+len};
    }
#endif

    ///////////////////////////////////////////////////////////////////////////////////////////////

    TimePoint TimePoint::now() noexcept
    {
        #if _WIN32
            // convert 100ns ticks to nanoseconds, this would overflow in 292 years
            return TimePoint{ ticks_to_ns(ticks_since_epoch()) };
        #elif RPP_FREERTOS
            return TimePoint{ freertos_get_time_ns() };
        #elif RPP_STM32_HAL
            return TimePoint{ cortex_m_get_time_ns() };
        #else
            struct timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            return TimePoint{ int64(t.tv_sec * NANOS_PER_SEC + t.tv_nsec) };
        #endif
    }

    TimePoint TimePoint::local() noexcept
    {
        #if _WIN32
            // convert 100ns ticks to nanoseconds, this would overflow in 292 years
            return TimePoint{ ticks_to_ns(ticks_since_epoch()) + timezone_offset_seconds() * NANOS_PER_SEC };
        #elif RPP_FREERTOS
            return TimePoint{freertos_get_time_ns() + timezone_offset_seconds() * NANOS_PER_SEC};
        #elif RPP_STM32_HAL
            return TimePoint{cortex_m_get_time_ns() + timezone_offset_seconds() * NANOS_PER_SEC};
        #else
            struct timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            return TimePoint{ int64(t.tv_sec * NANOS_PER_SEC + t.tv_nsec) + timezone_offset_seconds() * NANOS_PER_SEC };
        #endif
    }

    TimePoint TimePoint::utc_to_local() const noexcept
    {
        return TimePoint{ duration.nsec + timezone_offset_seconds() * NANOS_PER_SEC };
    }

    // TODO: this won't handle system time changes correctly, so we need an invalidation condition
    int64 TimePoint::timezone_offset_seconds() noexcept
    {
        static int64 timezone_offset = []() -> int64
        {
            time_t now = time(nullptr); // get current UTC time since epoch
            std::tm local_tm = localtime_safe(now); // convert to local calendar time with timezone offset and DST

            // convert local calendar time to time since epoch with timezone offset remaining
            // timegm() takes the TM struct as-is, without additional Timezone adjustment
            time_t local_now = timegm(&local_tm);
            time_t timezone_offset_sec = local_now - now;
            return int64(timezone_offset_sec); // e.g. 10800 if in UTC+3
        }();
        return timezone_offset;
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

extern "C" double time_now_seconds()
{
    return rpp::TimePoint::now().duration.sec();
}

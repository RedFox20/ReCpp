#pragma once
/**
 * Simple performance timer, Copyright (c) 2016-2017, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "config.h"
#include <string>

#if __cplusplus

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /** Let this thread sleep for provided MILLISECONDS */
    RPPAPI void sleep_ms(unsigned int millis) noexcept;

    /** Let this thread sleep for provided MICROSECONDS */
    RPPAPI void sleep_us(unsigned int micros) noexcept;

    /** Let this thread sleep for provided NANOSECONDS */
    RPPAPI void sleep_ns(uint64 nanos) noexcept;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    static constexpr int64 MILLIS_PER_SEC = 1'000LL;
    static constexpr int64 MICROS_PER_SEC = 1'000'000LL;
    static constexpr int64 NANOS_PER_SEC  = 1'000'000'000LL;
    static constexpr int64 NANOS_PER_MILLI = 1'000'000LL;
    static constexpr int64 NANOS_PER_MICRO = 1'000LL;
    static constexpr int64 NANOS_PER_YEAR   = 31'557'600'000'000'000LL;
    static constexpr int64 NANOS_PER_DAY    = 86'400'000'000'000LL;
    static constexpr int64 NANOS_PER_HOUR   = 3'600'000'000'000LL;
    static constexpr int64 NANOS_PER_MINUTE = 60'000'000'000LL;

    /**
     * @brief New Duration API for TimePoint arithmetics
     */
    struct Duration
    {
        // this has a maximum resolution of 9223372036 seconds (292.27 years)
        int64 nsec = 0;

        constexpr Duration() noexcept = default;
        explicit constexpr Duration(int64 ns) noexcept : nsec{ns} {}

        /**
         * @brief Constructs a new Duration from hours, minutes, seconds, nanos
         */
        constexpr Duration(int64 hours, int64 minutes, int64 seconds, int64 nanos) noexcept
            : nsec{ (hours   * NANOS_PER_HOUR)
                  + (minutes * NANOS_PER_MINUTE)
                  + (seconds * NANOS_PER_SEC)
                  + nanos }
        {
        }

        /** @brief The ZERO Duration */
        static constexpr Duration zero() noexcept { return {}; }
        static constexpr Duration max() noexcept { return Duration{ int64(RPP_INT64_MAX) }; }
        static constexpr Duration min() noexcept { return Duration{ int64(RPP_INT64_MIN) }; }
        explicit operator bool() const noexcept { return nsec != 0; }

        /** @returns true if this Duration has been initialized */
        constexpr bool is_valid() const noexcept { return nsec != 0; }
        constexpr bool operator==(const Duration& d) const noexcept { return nsec == d.nsec; }
        constexpr bool operator!=(const Duration& d) const noexcept { return nsec != d.nsec; }
        constexpr bool operator>(const Duration& d) const noexcept { return nsec > d.nsec; }
        constexpr bool operator<(const Duration& d) const noexcept { return nsec < d.nsec; }
        constexpr bool operator>=(const Duration& d) const noexcept { return nsec >= d.nsec; }
        constexpr bool operator<=(const Duration& d) const noexcept { return nsec <= d.nsec; }


        /** @returns New Duration from fractional seconds (positive or negative) */
        static constexpr Duration from_seconds(double seconds) noexcept { return Duration{ int64(seconds * NANOS_PER_SEC) }; }
        static constexpr Duration from_seconds(int32 seconds) noexcept { return Duration{ int64(seconds) * NANOS_PER_SEC }; }
        static constexpr Duration from_seconds(int64 seconds) noexcept { return Duration{ seconds * NANOS_PER_SEC }; }

        /** @returns New Duration from integer milliseconds (positive or negative) */
        static constexpr Duration from_millis(double millis) noexcept { return Duration{ int64(millis * NANOS_PER_MILLI) }; }
        static constexpr Duration from_millis(int32 millis) noexcept { return Duration{ int64(millis) * NANOS_PER_MILLI }; }
        static constexpr Duration from_millis(int64 millis) noexcept { return Duration{ millis * NANOS_PER_MILLI }; }

        /** @returns New Duration from integer microseconds (positive or negative) */
        static constexpr Duration from_micros(double micros) noexcept { return Duration{ int64(micros * NANOS_PER_MICRO) }; }
        static constexpr Duration from_micros(int32 micros) noexcept { return Duration{ int64(micros) * NANOS_PER_MICRO }; }
        static constexpr Duration from_micros(int64 micros) noexcept { return Duration{ micros * NANOS_PER_MICRO }; }

        /** @returns New Duration from integer nanoseconds (positive or negative) */
        static constexpr Duration from_nanos(int64 nanos) noexcept { return Duration{ nanos }; }

        /** @returns New Duration from integer days (positive or negative) */
        static constexpr Duration from_days(int32 days) noexcept { return Duration{ int64(days) * NANOS_PER_DAY }; }
        static constexpr Duration from_days(int64 days) noexcept { return Duration{ days * NANOS_PER_DAY }; }

        /** @returns New Duration from integer minutes (positive or negative) */
        static constexpr Duration from_minutes(int32 minutes) noexcept { return Duration{ int64(minutes) * NANOS_PER_MINUTE }; }
        static constexpr Duration from_minutes(int64 minutes) noexcept { return Duration{ minutes * NANOS_PER_MINUTE }; }

        /** @returns New Duration from integer hours (positive or negative) */
        static constexpr Duration from_hours(double hours) noexcept { return Duration{ int64(hours * NANOS_PER_HOUR) }; }
        static constexpr Duration from_hours(int32 hours) noexcept { return Duration{ int64(hours) * NANOS_PER_HOUR }; }
        static constexpr Duration from_hours(int64 hours) noexcept { return Duration{ hours * NANOS_PER_HOUR }; }

        /** 
         * @returns TOTAL fractional seconds (positive or negative) of this Duration
         */
        constexpr double sec() const noexcept { return double(nsec) / double(NANOS_PER_SEC); }
        /**
         * @returns TOTAL int64 seconds (positive or negative) of this Duration
         */
        constexpr int64 seconds() const noexcept { return nsec / NANOS_PER_SEC; }
        /**
         * @returns TOTAL int64 milliseconds (positive or negative) of this Duration
         */
        constexpr int64 millis() const noexcept { return nsec / NANOS_PER_MILLI; }
        /**
         * @returns TOTAL int64 microseconds (positive or negative) of this Duration
         */
        constexpr int64 micros() const noexcept { return nsec / NANOS_PER_MICRO; }
        /**
         * @returns TOTAL int64 nanoseconds (positive or negative) of this Duration
         * @warning This overflows at 9,223,372,036,854,775,807 nanoseconds (9,223,372,036 seconds, which is ~292 years)
         */
        constexpr int64 nanos() const noexcept { return nsec; }
        
        static constexpr int SECONDS_PER_YEAR = 31'557'600;

        /**
         * @returns TOTAL int64 days of this Duration
         */
        constexpr int64 days() const noexcept { return nsec / NANOS_PER_DAY; }
        /**
         * @returns TOTAL int64 hours of this Duration
         */
        constexpr int64 hours() const noexcept { return nsec / NANOS_PER_HOUR; }
        /**
         * @returns TOTAL int64 minutes of this Duration
         */
        constexpr int64 minutes() const noexcept { return nsec / NANOS_PER_MINUTE; }

        /**
         * @returns This time duration as an absolute duration
         */
        constexpr Duration abs() const noexcept { return Duration{ nsec < 0 ? -nsec : nsec }; }

        /**
         * @returns This duration clamped between [min, max] range.
         */
        constexpr Duration clamped(const Duration& min, const Duration& max) const noexcept
        {
            int64 nsec_clamped = nsec;
            if      (nsec_clamped < min.nsec) nsec_clamped = min.nsec;
            else if (nsec_clamped > max.nsec) nsec_clamped = max.nsec;
            return Duration{ nsec_clamped };
        }

        constexpr Duration operator+(const Duration& d) const noexcept { return Duration{ nsec + d.nsec }; }
        constexpr Duration operator-(const Duration& d) const noexcept { return Duration{ nsec - d.nsec }; }
        constexpr Duration& operator+=(const Duration& d) noexcept { nsec += d.nsec; return *this; }
        constexpr Duration& operator-=(const Duration& d) noexcept { nsec -= d.nsec; return *this; }
        constexpr Duration operator-() const noexcept { return Duration{ -nsec }; }

        /** @brief Divides this duration by an integer */
        constexpr Duration operator/(int32 d) const noexcept { return Duration{ nsec / d }; }
        /**
         * @brief Multiplies this duration by an integer
         * @warning This can overflow!!!
         */
        constexpr Duration operator*(int32 d) const noexcept { return Duration{ nsec * d }; }

        /**
         * @brief Multiplies this duration by a fraction - must be double precision due to internal int64 representation
         * @warning This can overflow if the double is too large!!!
         */
        constexpr Duration operator*(double d) const noexcept { return Duration{ int64(double(nsec) * d) }; }

        /** 
         * @brief Converts this Duration into a string with fractional seconds
         * @example "12:44:00" -- 0 fractional digits
         * @example "12:44:00.938" -- 3 fractional digits (milliseconds)
         * @example "12:44:00.937521" -- 6 fractional digits (microseconds)
         * @example "12:44:00.937521423" -- 9 fractional digits (nanoseconds)
         * @param buf            - Output buffer [must be at least 25 bytes]
         * @param bufsize        - Size of the output buffer
         * @param fraction_digits - Number of fractional digits to include (default is 3 which is milliseconds) (6 is microseconds)
         * @returns Number of characters written into the buffer
         */
        int to_string(char* buf, int bufsize, int fraction_digits = 3/*3 digits -- milliseconds*/) const noexcept;

        std::string to_string(int fraction_digits = 3/*3 digits -- milliseconds*/) const noexcept;

        /**
         * @brief Converts this Duration into a Stopwatch string which is easier for humans to read.
         *        The fraction_digits parameter decides where microseconds or nanoseconds are also displayed.
         * @example "[100m 12s 44ms]" -- 3 fractional digits
         * @example "[100m 12s 44ms 938us]" -- 6 fractional digits
         * @example "[100m 12s 44ms 937us 521ns]" -- 9 fractional digits
         * @param buf            - Output buffer [must be at least 36 bytes]
         * @param bufsize        - Size of the output buffer
         * @param fraction_digits - Number of fractional digits to include (default is 3 which is milliseconds)
         * @returns Number of characters written into the buffer
         */
        int to_stopwatch_string(char* buf, int bufsize, int fraction_digits = 3) const noexcept;

        std::string to_stopwatch_string(int fraction_digits = 3) const noexcept;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief New TimePoint API for OS specific high accuracy timepoints
     *        which avoids floating point calculations.
     *
     * @note This TimePoint is synchronized to UNIX epoch since 1970,
     *       however it is not designed for datetime calculations.
     *       The main purpose of this TimePoint is to provide high accuracy time measurements.
     *       For datetime calculations use the DateTime class.
     */
    struct TimePoint
    {
        // public duration object, feel free to set it to whatever you want
        Duration duration;

        constexpr TimePoint() = default;
        constexpr explicit TimePoint(const Duration& d) noexcept : duration{d} {}
        constexpr explicit TimePoint(int64 ns) noexcept : duration{ns} {}

        /**
         * @brief Constructs a new TimePoint from year, month, day, hour, minute, second, nanos
         */
        NOINLINE TimePoint(int year, int month, int day, int hour, int minute, int second, int64 nanos) noexcept;

        /** @brief The ZERO TimePoint */
        static constexpr TimePoint zero() noexcept { return {}; }
        static constexpr TimePoint max() noexcept { return TimePoint{ int64(RPP_INT64_MAX) }; }
        static constexpr TimePoint min() noexcept { return TimePoint{ int64(RPP_INT64_MIN) }; }

        /** @returns TimePoint from a UNIX EPOCH microseconds timestamp */
        static constexpr TimePoint from_epoch_us(uint64 unix_epoch_us) noexcept { return TimePoint{ int64(unix_epoch_us) * NANOS_PER_MICRO }; }

        /** @returns Current OS specific high accuracy timepoint */
        static TimePoint now() noexcept;

        /** @returns Current OS time with timezone offset */
        static TimePoint local() noexcept;

        /** @returns only the HH:MM:SS.NANOS part of the Duration */
        constexpr Duration time_of_day() const noexcept { return Duration{ duration.nsec % NANOS_PER_DAY }; }

        /** @returns Duration from this point until end */
        constexpr Duration elapsed(const TimePoint& end) const noexcept { return Duration{end.duration.nsec-duration.nsec}; }

        /** @returns fractional seconds elapsed from this time point until end */
        constexpr double elapsed_sec(const TimePoint& end) const noexcept { return Duration{end.duration.nsec-duration.nsec}.sec(); }
        /** @returns integer seconds elapsed from this time point until end */
        constexpr int64 elapsed_s(const TimePoint& end) const noexcept { return Duration{end.duration.nsec-duration.nsec}.seconds(); }
        /** @returns integer milliseconds elapsed from this time point until end */
        constexpr int64 elapsed_ms(const TimePoint& end) const noexcept { return Duration{end.duration.nsec-duration.nsec}.millis(); }
        /** @returns integer microseconds elapsed from this time point until end */
        constexpr int64 elapsed_us(const TimePoint& end) const noexcept { return Duration{end.duration.nsec-duration.nsec}.micros(); }
        /** @returns integer nanoseconds elapsed from this time point until end */
        constexpr int64 elapsed_ns(const TimePoint& end) const noexcept { return Duration{end.duration.nsec-duration.nsec}.nanos(); }

        /** @returns true if this timepoint has been initialized */
        constexpr bool is_valid() const noexcept { return duration.is_valid(); }
        constexpr explicit operator bool() const noexcept { return duration.is_valid(); }

        constexpr bool operator==(const TimePoint& t) const noexcept { return duration.nsec == t.duration.nsec; }
        constexpr bool operator!=(const TimePoint& t) const noexcept { return duration.nsec != t.duration.nsec; }
        constexpr bool operator>(const TimePoint& t) const noexcept { return duration.nsec > t.duration.nsec; }
        constexpr bool operator<(const TimePoint& t) const noexcept { return duration.nsec < t.duration.nsec; }
        constexpr bool operator>=(const TimePoint& t) const noexcept { return duration.nsec >= t.duration.nsec; }
        constexpr bool operator<=(const TimePoint& t) const noexcept { return duration.nsec <= t.duration.nsec; }
        constexpr TimePoint operator+(const Duration& d) const noexcept { return TimePoint{duration.nsec + d.nsec}; }
        constexpr TimePoint operator-(const Duration& d) const noexcept { return TimePoint{duration.nsec - d.nsec}; }
        constexpr TimePoint& operator+=(const Duration& d) noexcept { duration.nsec += d.nsec; return *this; }
        constexpr TimePoint& operator-=(const Duration& d) noexcept { duration.nsec -= d.nsec; return *this;}
        constexpr Duration operator-(const TimePoint& t) const noexcept { return Duration{duration.nsec - t.duration.nsec}; }

        /** @brief Converts this Duration into a string */
        int to_string(char* buf, int bufsize, int fraction_digits = 3/*3 digits -- milliseconds*/) const noexcept;
        std::string to_string(int fraction_digits = 3/*3 digits -- milliseconds*/) const noexcept;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * High accuracy timer for performance profiling or deltaTime measurement
     */
    struct RPPAPI Timer
    {
        // public started timepoint, feel free to set it to whatever you want
        TimePoint started;

        enum StartMode {
            NoStart,
            AutoStart, // default behaviour
        };

        /** Initializes a new timer by calling start */
        Timer() noexcept;

        /** @brief Initializes timer with either NoStart or AutoStart modes */
        explicit Timer(StartMode mode) noexcept;

        /** @returns true if this timer has been started */
        bool is_started() const noexcept { return started.is_valid(); }

        /** Starts the timer */
        void start() noexcept { started = TimePoint::now(); }

        /** @brief Resets the timer to a custom time point */
        void reset(const TimePoint& time = TimePoint::zero()) noexcept { started = time; }

        /** @return Fractional seconds elapsed from start() */
        double elapsed() const noexcept;
        /** @return Fractional milliseconds elapsed from start() */
        double elapsed_millis() const noexcept;
        /** @return Fractional microseconds elapsed from start() */
        double elapsed_micros() const noexcept;

        /** Gets the next time sample, since the last call to next() or start() and calls start() again */
        double next() noexcept;
        /** @return next() converted to milliseconds */
        double next_millis() noexcept;

        /** @returns integer milliseconds elapsed from start() until end */
        int64 elapsed_ms(const TimePoint& end) const noexcept { return started.elapsed_ms(end); }
        /** @returns integer microseconds elapsed from start() until end */
        int64 elapsed_us(const TimePoint& end) const noexcept { return started.elapsed_us(end); }
        /** @returns integer nanoseconds elapsed from start() until end */
        int64 elapsed_ns(const TimePoint& end) const noexcept { return started.elapsed_ns(end); }

        /** @returns integer milliseconds elapsed from start() until now() */
        int64 elapsed_ms() const noexcept { return started.elapsed_ms(TimePoint::now()); }
        /** @returns integer microseconds elapsed from start() until now() */
        int64 elapsed_us() const noexcept { return started.elapsed_us(TimePoint::now()); }
        /** @returns integer microseconds elapsed from start() until now() */
        int64 elapsed_ns() const noexcept { return started.elapsed_ns(TimePoint::now()); }

        /** Measure block execution time in fractional seconds */
        template<class Func> static double measure(const Func& f)
        {
            Timer t;
            f();
            return t.elapsed();
        }

        /** Measure block execution time in fractional milliseconds */
        template<class Func> static double measure_millis(const Func& f)
        {
            Timer t;
            f();
            return t.elapsed_millis();
        }
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * High accuracy stopwatch for measuring specific events and keeping the results
     */
    struct RPPAPI StopWatch
    {
        TimePoint begin;
        TimePoint end;

        /** Creates an uninitialized StopWatch. Reported time is always 0.0 */
        StopWatch() noexcept = default;

        /** @brief Creates a new StopWatch and starts it immediately */
        static StopWatch start_new() noexcept { StopWatch sw; sw.start(); return sw; }

        /** 
         * Sets the initial starting point of the stopwatch and resets the stop point
         * only if the stopwatch hasn't started already
         * @note No effect if started()
         */
        void start() noexcept;

        /**
         * Sets the stop point of the stopwatch only if start point
         * exists and not already stopped
         * @note No effect if !started() || stopped()
         */
        void stop() noexcept;

        /** Clears the stop point and resumes timing */
        void resume() noexcept;

        /** Clears both start and stop times */
        void clear() noexcept;

        /** Clears the timer and starts it again */
        void restart() noexcept;

        /** Has the stopwatch been started? */
        bool started() const noexcept { return begin.is_valid(); }

        /** Has the stopwatch been stopped with a valid time? */
        bool stopped() const noexcept { return end.is_valid(); }

        /** 
         * Reports the currently elapsed time. 
         * If stopwatch is stopped, it will report the stored time
         * If stopwatch is still running, it will report the currently elapsed time
         * Otherwise reports 0.0
         */
        double elapsed() const noexcept;

        /** Currently elapsed time in milliseconds */
        double elapsed_millis() const noexcept { return elapsed() * 1000.0; }
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Automatically logs performance from constructor to destructor and writes it to log
     */
    class RPPAPI ScopedPerfTimer
    {
        const char* prefix;   // log prefix:   "[perf]"
        const char* location; // funcname:     "someFunction()"
        const char* detail;   // detail info:  currentItem.name.c_str()
        TimePoint start;
    public:
        /**
         * Scoped performance timer with optional prefix, function name and detail info
         * @param prefix Prefix used in log, for example "[perf]"
         * @param location Location name where time is being measured
         * @param detail Detailed info of which item is being measured, for example an item name
         */
        ScopedPerfTimer(const char* prefix, const char* location, const char* detail) noexcept;
        ScopedPerfTimer(const char* prefix, const char* location) noexcept
            : ScopedPerfTimer{prefix, location, nullptr} {}
    #if RPP_HAS_CXX20 || _MSC_VER >= 1926 || defined(__GNUC__)
        explicit ScopedPerfTimer(const char* location = __builtin_FUNCTION()) noexcept
            : ScopedPerfTimer{"[perf]", location, nullptr} {}
    #else
        ScopedPerfTimer() noexcept
            : ScopedPerfTimer{"[perf]", "unknown location", nullptr} {}
        /** @brief For backwards compatibility */
        explicit ScopedPerfTimer(const char* location) noexcept
            : ScopedPerfTimer{"[perf]", location, nullptr} {}
    #endif
        ~ScopedPerfTimer() noexcept;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////
}
#endif

#if __cplusplus
extern "C" {
#endif
    /** @return Current time in fractional seconds */
    RPPAPI double time_now_seconds();
#if __cplusplus
}
#endif


#pragma once
/**
 * Simple performance timer, Copyright (c) 2016-2017, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <stdint.h>
#include "config.h"

#if __cplusplus

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /** Let this thread sleep for provided MILLISECONDS */
    RPPAPI void sleep_ms(unsigned int millis) noexcept;

    /** Let this thread sleep for provided MICROSECONDS */
    RPPAPI void sleep_us(unsigned int micros) noexcept;

    /** Let this thread sleep for provided NANOSECONDS */
    RPPAPI void sleep_ns(uint64_t nanos) noexcept;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    static constexpr int64_t MILLIS_PER_SEC = 1'000LL;
    static constexpr int64_t MICROS_PER_SEC = 1'000'000LL;
    static constexpr int64_t NANOS_PER_SEC  = 1'000'000'000LL;
    static constexpr int64_t NANOS_PER_MILLI = 1'000'000LL;
    static constexpr int64_t NANOS_PER_MICRO = 1'000LL;

    /**
     * @brief New Duration API for TimePoint arithmetics
     */
    struct Duration
    {
        // this has a maximum resolution of 9223372036 seconds (292.27 years)
        int64_t nsec = 0;

        Duration() noexcept = default;
        explicit constexpr Duration(int64_t ns) noexcept : nsec{ns} {}

        /** @brief The ZERO Duration */
        static constexpr Duration zero() noexcept { return {}; }
        explicit operator bool() const noexcept { return nsec != 0; }

        /** @returns true if this Duration has been initialized */
        bool is_valid() const noexcept { return nsec != 0; }
        bool operator==(const Duration& d) const noexcept { return nsec == d.nsec; }
        bool operator!=(const Duration& d) const noexcept { return nsec != d.nsec; }
        bool operator>(const Duration& d) const noexcept { return nsec > d.nsec; }
        bool operator<(const Duration& d) const noexcept { return nsec < d.nsec; }


        /** @returns New Duration from fractional seconds (positive or negative) */
        static Duration from_seconds(double seconds) noexcept { return Duration{ int64_t(seconds * NANOS_PER_SEC) }; }
        /** @returns New Duration from integer milliseconds (positive or negative) */
        static Duration from_millis(int32_t millis) noexcept { return Duration{ int64_t(millis * NANOS_PER_MILLI) }; }
        /** @returns New Duration from integer microseconds (positive or negative) */
        static Duration from_micros(int32_t micros) noexcept { return Duration{ int64_t(micros * NANOS_PER_MICRO ) }; }
        /** @returns New Duration from integer nanoseconds (positive or negative) */
        static Duration from_nanos(int64_t nanos) noexcept { return Duration{ nanos }; }


        /** 
         * @returns fractional seconds (positive or negative) of this Duration
         */
        double seconds() const noexcept { return double(nsec) / double(NANOS_PER_SEC); }
        /**
         * @returns integer milliseconds (positive or negative) of this Duration
         * @warning This overflows at 2,147,483,647 milliseconds (2,147,483 seconds, which is 24 days)
         */
        int32_t millis() const noexcept { return int32_t(nsec / NANOS_PER_MILLI); }
        /**
         * @returns integer microseconds (positive or negative) of this Duration
         * @warning This overflows at 2,147,483,647 microseconds (2,147 seconds, which is 35 minutes)
         */
        int32_t micros() const noexcept { return int32_t(nsec / NANOS_PER_MICRO); }
        /**
         * @returns integer nanoseconds (positive or negative) of this Duration
         * @warning This overflows at 9,223,372,036,854,775,807 nanoseconds (9,223,372,036 seconds, which is 292,471 years)
         */
        int64_t nanos() const noexcept { return nsec; }


        Duration operator+(const Duration& d) const noexcept { return Duration{ nsec + d.nsec }; }
        Duration operator-(const Duration& d) const noexcept { return Duration{ nsec - d.nsec }; }
        Duration& operator+=(const Duration& d) noexcept { nsec += d.nsec; return *this; }
        Duration& operator-=(const Duration& d) noexcept { nsec -= d.nsec; return *this; }
        Duration operator-() const noexcept { return Duration{ -nsec }; }

        /** @brief Divides this duration by an integer */
        Duration operator/(int32_t d) const noexcept { return Duration{ nsec / d }; }
        /**
         * @brief Multiplies this duration by an integer
         * @warning This can overflow!!!
         */
        Duration operator*(int32_t d) const noexcept { return Duration{ nsec * d }; }
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief New TimePoint API for OS specific high accuracy timepoints
     *        which avoids floating point calculations.
     * @note This time point is not synchronized to any external clocks
     *       and is only useful for measuring time deltas.
     */
    struct TimePoint
    {
        // public duration object, feel free to set it to whatever you want
        Duration duration;

        TimePoint() = default;
        constexpr explicit TimePoint(const Duration& d) noexcept : duration{d} {}
        constexpr explicit TimePoint(int64_t ns) noexcept : duration{ns} {}

        /** @brief The ZERO TimePoint */
        static constexpr TimePoint zero() noexcept { return {}; }

        /** @returns Current OS specific high accuracy timepoint */
        static TimePoint now() noexcept;

        /** @returns Duration from this point until end */
        Duration elapsed(const TimePoint& end) const noexcept { return (end.duration - duration); }

        /** @returns fractional seconds elapsed from this time point until end */
        double elapsed_sec(const TimePoint& end) const noexcept { return (end.duration - duration).seconds(); }
        /** @returns integer milliseconds elapsed from this time point until end */
        int32_t elapsed_ms(const TimePoint& end) const noexcept { return (end.duration - duration).millis(); }
        /** @returns integer microseconds elapsed from this time point until end */
        int32_t elapsed_us(const TimePoint& end) const noexcept { return (end.duration - duration).micros(); }
        /** @returns integer nanoseconds elapsed from this time point until end */
        int64_t elapsed_ns(const TimePoint& end) const noexcept { return (end.duration - duration).nanos(); }

        /** @returns true if this timepoint has been initialized */
        bool is_valid() const noexcept { return duration.is_valid(); }
        explicit operator bool() const noexcept { return duration.is_valid(); }

        bool operator==(const TimePoint& t) const noexcept { return duration == t.duration; }
        bool operator!=(const TimePoint& t) const noexcept { return duration != t.duration; }
        bool operator>(const TimePoint& t) const noexcept { return duration > t.duration; }
        bool operator<(const TimePoint& t) const noexcept { return duration < t.duration; }
        TimePoint operator+(const Duration& d) const noexcept { return TimePoint{duration + d}; }
        TimePoint operator-(const Duration& d) const noexcept { return TimePoint{duration - d}; }
        TimePoint& operator+=(const Duration& d) noexcept { duration += d; return *this; }
        TimePoint& operator-=(const Duration& d) noexcept { duration -= d; return *this;}
        Duration operator-(const TimePoint& t) const noexcept { return duration - t.duration; }
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
        void reset(const TimePoint& time) noexcept { started = time; }

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
        int32_t elapsed_ms(const TimePoint& end) const noexcept { return started.elapsed_ms(end); }
        /** @returns integer microseconds elapsed from start() until end */
        int32_t elapsed_us(const TimePoint& end) const noexcept { return started.elapsed_us(end); }
        /** @returns integer nanoseconds elapsed from start() until end */
        int64_t elapsed_ns(const TimePoint& end) const noexcept { return started.elapsed_ns(end); }

        /** @returns integer milliseconds elapsed from start() until now() */
        int32_t elapsed_ms() const noexcept { return started.elapsed_ms(TimePoint::now()); }
        /** @returns integer microseconds elapsed from start() until now() */
        int32_t elapsed_us() const noexcept { return started.elapsed_us(TimePoint::now()); }
        /** @returns integer microseconds elapsed from start() until now() */
        int64_t elapsed_ns() const noexcept { return started.elapsed_ns(TimePoint::now()); }

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

        /** Resets both start and stop times */
        void reset() noexcept;

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


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

    /**
     * Gets the current timestamp from the system's most accurate time measurement
     */
    RPPAPI uint64_t time_now() noexcept;

    /**
     * Gets the time period for the system's most accurate time measurement
     * To convert time_now() to seconds:  double sec = time_now() * time_period() 
     */
    RPPAPI double time_period() noexcept;

    /** Converts fractional seconds to clock ticks that matches time_now() */
    RPPAPI int64_t from_sec_to_time_ticks(double seconds) noexcept;
    /** Converts fractional milliseconds to clock ticks that matches time_now() */
    RPPAPI int64_t from_ms_to_time_ticks(double millis) noexcept;
    /** Converts fractional microseconds to clock ticks that matches time_now() */
    RPPAPI int64_t from_us_to_time_ticks(double micros) noexcept;
    /** Converts fractional nanoseconds to clock ticks that matches time_now() */
    RPPAPI int64_t from_ns_to_time_ticks(double nanos) noexcept;

    /** Converts clock ticks that matches time_now() into fractional seconds */
    RPPAPI double time_ticks_to_sec(int64_t ticks) noexcept;
    /** Converts clock ticks that matches time_now() into fractional milliseconds */
    RPPAPI double time_ticks_to_ms(int64_t ticks) noexcept;
    /** Converts clock ticks that matches time_now() into fractional microseconds */
    RPPAPI double time_ticks_to_us(int64_t ticks) noexcept;
    /** Converts clock ticks that matches time_now() into fractional nanoseconds */
    RPPAPI double time_ticks_to_ns(int64_t ticks) noexcept;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /** Let this thread sleep for provided MILLISECONDS */
    RPPAPI void sleep_ms(unsigned int millis) noexcept;

    /** Let this thread sleep for provided MICROSECONDS */
    RPPAPI void sleep_us(unsigned int micros) noexcept;

    /** Let this thread sleep for provided NANOSECONDS */
    RPPAPI void sleep_ns(uint64_t nanos) noexcept;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief New TimePoint API for OS specific high accuracy timepoints
     *        which avoids floating point calculations
     */
    struct TimePoint
    {
        #if _WIN32 || __APPLE__
            uint64_t ticks = 0;
        #else
            #if __ANDROID__
                time_t sec = 0;
            #elif defined(__USE_TIME_BITS64)
                __time64_t sec = 0;
            #else
                __time_t sec = 0;
            #endif
                long int nanos = 0;
        #endif

        /** @brief The ZERO TimePoint */
        static constexpr TimePoint zero() noexcept { return {}; }

        /** @returns Current OS specific high accuracy timepoint */
        static TimePoint now() noexcept;

        /** @returns fractional seconds elapsed from this time point until end */
        double elapsed_sec(const TimePoint& end) const noexcept;

        /** @returns integer milliseconds elapsed from this time point until end */
        uint32_t elapsed_ms(const TimePoint& end) const noexcept;

        /** @returns integer microseconds elapsed from this time point until end */
        uint32_t elapsed_us(const TimePoint& end) const noexcept;

        /** @returns integer nanoseconds elapsed from this time point until end */
        uint32_t elapsed_ns(const TimePoint& end) const noexcept;

        /** @returns true if this timepoint has been initialized */
        bool is_valid() const noexcept
        {
            #if _WIN32 || __APPLE__
                return ticks != 0;
            #else 
                return sec != 0 || nanos != 0;
            #endif
        }

        explicit operator bool() const noexcept { return is_valid(); }

        bool operator==(const TimePoint& t) const noexcept
        {
            #if _WIN32 || __APPLE__
                return ticks == t.ticks;
            #else
                return sec == t.sec && nanos == t.nanos;
            #endif
        }

        bool operator!=(const TimePoint& t) const noexcept
        {
            #if _WIN32 || __APPLE__
                return ticks != t.ticks;
            #else
                return sec != t.sec || nanos != t.nanos;
            #endif
        }
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
        double elapsed_ms() const noexcept { return elapsed() * 1000.0; }

        /** Gets the next time sample, since the last call to next() or start() and calls start() again */
        double next() noexcept;

        /** @return next() converted to milliseconds */
        double next_ms() noexcept { return next() * 1000.0; }

        /** @returns integer milliseconds elapsed from this time point until end */
        uint32_t elapsed_ms(const TimePoint& end) const noexcept { return started.elapsed_ms(end); }

        /** @returns integer microseconds elapsed from this time point until end */
        uint32_t elapsed_us(const TimePoint& end) const noexcept { return started.elapsed_us(end); }

        /** @returns integer nanoseconds elapsed from this time point until end */
        uint32_t elapsed_ns(const TimePoint& end) const noexcept { return started.elapsed_ns(end); }

        /** Measure block execution time as seconds */
        template<class Func> static double measure(const Func& f)
        {
            Timer t;
            f();
            return t.elapsed();
        }

        /** Measure block execution time as milliseconds */
        template<class Func> static double measure_ms(const Func& f)
        {
            Timer t;
            f();
            return t.elapsed_ms();
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
        StopWatch() = default;

        /** 
         * Sets the initial starting point of the stopwatch and resets the stop point
         * only if the stopwatch hasn't started already
         * @note No effect if started()
         */
        void start();

        /**
         * Sets the stop point of the stopwatch only if start point
         * exists and not already stopped
         * @note No effect if !started() || stopped()
         */
        void stop();

        /** Clears the stop point and resumes timing */
        void resume();

        /** Resets both start and stop times */
        void reset();

        /** Has the stopwatch been started? */
        bool started() const { return begin.is_valid(); }

        /** Has the stopwatch been stopped with a valid time? */
        bool stopped() const { return end.is_valid(); }

        /** 
         * Reports the currently elapsed time. 
         * If stopwatch is stopped, it will report the stored time
         * If stopwatch is still running, it will report the currently elapsed time
         * Otherwise reports 0.0
         */
        double elapsed() const;

        /** Currently elapsed time in milliseconds */
        double elapsed_ms() const { return elapsed() * 1000.0; }
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
    
    /**
     * @return Current time in seconds:  rpp::time_now() * rpp::time_period();
     */
    RPPAPI double time_now_seconds();

#if __cplusplus
}
#endif


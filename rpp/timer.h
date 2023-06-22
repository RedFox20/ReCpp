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

    /** Let this thread sleep for provided MILLISECONDS */
    RPPAPI void sleep_ms(unsigned int millis) noexcept;

    /** Let this thread sleep for provided MICROSECONDS */
    RPPAPI void sleep_us(unsigned int micros) noexcept;

    /** Let this thread sleep for provided NANOSECONDS */
    RPPAPI void sleep_ns(uint64_t nanos) noexcept;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * High accuracy timer for performance profiling or deltaTime measurement
     */
    struct RPPAPI Timer
    {
        uint64_t value;
        
        enum StartMode {
            NoStart,
            AutoStart, // default behaviour
        };

        /** Initializes a new timer by calling start */
        Timer() noexcept;
        
        explicit Timer(StartMode startMode) noexcept;

        /** Starts the timer */
        void start() noexcept;

        /** @return Fractional seconds elapsed from start() */
        double elapsed() const noexcept;

        /** @return Fractional milliseconds elapsed from start() */
        double elapsed_ms() const noexcept { return elapsed() * 1000.0; }

        /** Gets the next time sample, since the last call to next() or start() and calls start() again */
        double next() noexcept;

        /** @return next() converted to milliseconds */
        double next_ms() noexcept { return next() * 1000.0; }

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
        uint64_t begin = 0;
        uint64_t end   = 0;

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
        bool started() const { return begin != 0; }

        /** Has the stopwatch been stopped with a valid time? */
        bool stopped() const { return end != 0; }

        /** 
         * Reports the currently elapsed time. 
         * If stopwatch is stopped, it will report the stored time
         * If stopwatch is still running, it will report the currently elapsed time
         * Otherwise reports 0.0
         */
        double elapsed() const;

        /** Currently elpased time in milliseconds */
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
        uint64_t start;
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


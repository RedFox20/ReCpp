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
    int64_t from_sec_to_time_ticks(double seconds) noexcept;
    /** Converts fractional milliseconds int to clock ticks that matches time_now() */
    int64_t from_ms_to_time_ticks(double millis) noexcept;
    /** Converts fractional microseconds int to clock ticks that matches time_now() */
    int64_t from_us_to_time_ticks(double micros) noexcept;

    /** Converts clock ticks that matches time_now() into fractional seconds */
    double time_ticks_to_sec(int64_t ticks) noexcept;
    /** Converts clock ticks that matches time_now() into fractional milliseconds */
    double time_ticks_to_ms(int64_t ticks) noexcept;
    /** Converts clock ticks that matches time_now() into fractional microseconds */
    double time_ticks_to_us(int64_t ticks) noexcept;

    /** Let this thread sleep for provided milliseconds */
    RPPAPI void sleep_ms(unsigned int millis) noexcept;

    /** Let this thread sleep for provided MICROSECONDS */
    RPPAPI void sleep_us(unsigned int microseconds) noexcept;

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

        /** @return Number of seconds elapsed from start(); */
        double elapsed() const noexcept;

        /** @return Number of milliseconds elapsed from start(); */
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
        Timer timer;
        const char* what;
    public:
        ScopedPerfTimer(const char* what) noexcept ;
        ~ScopedPerfTimer() noexcept ;
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


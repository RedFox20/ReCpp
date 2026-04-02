#pragma once
/**
 * Simple performance Timers and high performance Duration/TimePoint, Copyright (c) 2016-2025, Jorma Rebane
 * Distributed under MIT Software License
 */
#include "timepoint.h" // Duration, TimePoint, sleep functions, time constants, duration literals

#if __cplusplus

namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * High accuracy timer for performance profiling or deltaTime measurement
     * Uses monotonic time by default.
     */
    struct RPPAPI Timer
    {
        // public started timepoint, feel free to set it to whatever you want
        TimePoint started;

        static constexpr ClockType DefaultClock = ClockType::Monotonic; // default clock type for timers

        // clock type used by this timer for all now() calls
        ClockType clock = DefaultClock;

        enum StartMode {
            NoStart,
            AutoStart, // default behaviour
        };

        /** Initializes a new timer by calling start */
        Timer() noexcept;

        /** @brief Initializes timer with a specific clock type */
        explicit Timer(ClockType clock) noexcept;

        /** @brief Initializes timer with either NoStart or AutoStart modes */
        explicit Timer(StartMode mode) noexcept;

        /** @brief Initializes timer with a specific clock type and start mode */
        Timer(ClockType clock, StartMode mode) noexcept;

        /** @returns true if this timer has been started */
        bool is_started() const noexcept { return started.is_valid(); }

        /** @returns The current time using this timer's clock type */
        TimePoint time_now() const noexcept { return TimePoint::now(clock); }

        /** Starts the timer */
        void start() noexcept { started = time_now(); }

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
        int64 elapsed_ms() const noexcept { return started.elapsed_ms(time_now()); }
        /** @returns integer microseconds elapsed from start() until now() */
        int64 elapsed_us() const noexcept { return started.elapsed_us(time_now()); }
        /** @returns integer microseconds elapsed from start() until now() */
        int64 elapsed_ns() const noexcept { return started.elapsed_ns(time_now()); }

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
     *
     * @code
     * void slow_function()
     * {
     *     ScopedPerfTimer timer{"[perf]", __FUNCTION__, 500};
     *     do_slow_work();
     *     // reports elapsed time to log if > 500us elapsed
     * }
     * @endcode
     */
    class RPPAPI ScopedPerfTimer
    {
        const char* prefix;   // log prefix:   "[perf]"
        const char* location; // funcname:     "someFunction()"
        const char* detail;   // detail info:  currentItem.name.c_str()
        TimePoint start;
        unsigned long threshold_us; // microseconds threshold to trigger the logging
    public:
        /**
         * Scoped performance timer with optional prefix, function name and detail info
         * @param prefix Prefix used in log, for example "[perf]"
         * @param location Location name where time is being measured
         * @param detail Detailed info of which item is being measured, for example an item name
         */
        ScopedPerfTimer(const char* prefix, const char* location, const char* detail, unsigned long threshold_us = 0) noexcept;
        ScopedPerfTimer(const char* prefix, const char* location, unsigned long threshold_us = 0) noexcept
            : ScopedPerfTimer{prefix, location, nullptr, threshold_us} {}
    #if RPP_HAS_CXX20 || _MSC_VER >= 1926 || defined(__GNUC__)
        explicit ScopedPerfTimer(const char* location = __builtin_FUNCTION()) noexcept
            : ScopedPerfTimer{"[perf]", location, nullptr, 0} {}
    #else
        ScopedPerfTimer() noexcept
            : ScopedPerfTimer{"[perf]", "unknown location", nullptr, 0} {}
        /** @brief For backwards compatibility */
        explicit ScopedPerfTimer(const char* location) noexcept
            : ScopedPerfTimer{"[perf]", location, nullptr, 0} {}
    #endif
        ~ScopedPerfTimer() noexcept;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////
}
#endif



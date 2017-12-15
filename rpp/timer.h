#pragma once
/**
 * Simple performance timer, Copyright (c) 2016-2017, Jorma Rebane
 * Distributed under MIT Software License
 */
#include <stdint.h>

#ifndef RPPAPI
#  if _MSC_VER
#    define RPPAPI __declspec(dllexport)
#  else // clang/gcc
#    define RPPAPI __attribute__((visibility("default")))
#  endif
#endif

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

        /** Initializes a new timer by calling start */
        Timer() noexcept;

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


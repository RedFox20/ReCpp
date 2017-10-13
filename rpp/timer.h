#pragma once
#ifndef RPP_TIMER_H
#define RPP_TIMER_H
/**
 * Simple performance timer, Copyright (c) 2017 - Jorma Rebane
 */
#include <stdint.h>

#if __cplusplus
namespace rpp
{
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Gets the current timestamp from the system's most accurate time measurement
     */
    uint64_t time_now() noexcept;

    /**
     * Gets the time period for the system's most accurate time measurement
     * To convert time_now() to seconds:  double sec = time_now() * time_period() 
     */
    double time_period() noexcept;

    /** Let this thread sleep for provided milliseconds */
    void sleep_ms(unsigned int millis) noexcept;

    /** Let this thread sleep for provided MICROSECONDS */
    void sleep_us(unsigned int microseconds) noexcept;

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * High accuracy timer for performance profiling or deltaTime measurement
     */
    struct Timer
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
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Automatically logs performance from constructor to destructor and writes it to log
     */
    class ScopedPerfTimer
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
    double time_now_seconds();

#if __cplusplus
}
#endif


#endif // RPP_TIMER_H

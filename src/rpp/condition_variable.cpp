#include "condition_variable.h"
/**
 * This implementation is based on the C++ standard std::condition_variable,
 * and implements Spin-wait timeouts in wait_for() and wait_until(),
 * which allows timeouts smaller than ~15.6ms (default timer tick) on Windows.
 *
 * For UNIX platforms, the standard std::condition_variable is used
 *
 * Copyright (c) 2023, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <realtimeapiset.h>
#include <timeapi.h>
#pragma comment(lib, "Winmm.lib") // MM time library
#pragma comment(lib, "Mincore.lib")

namespace rpp
{
    #define CONDVAR(handle) PCONDITION_VARIABLE(&handle)

    condition_variable::condition_variable() noexcept
    {
        // Win32 COND VARS are allocated once and never deleted
        InitializeConditionVariable(CONDVAR(handle));
    }

    condition_variable::~condition_variable() noexcept
    {
    }

    void condition_variable::notify_one() noexcept
    {
        // this lock is needed, otherwise waiters might lock mutex and enter wait state
        // right after this signal is sent, causing a deadlock
        cs.lock();
        WakeConditionVariable(CONDVAR(handle));
        cs.unlock();
    }

    void condition_variable::notify_all() noexcept
    {
        // this lock is needed, otherwise waiters might lock mutex and enter wait state
        // right after this signal is sent, causing a deadlock
        cs.lock();
        WakeAllConditionVariable(CONDVAR(handle));
        cs.unlock();
    }

    std::cv_status condition_variable::_wait_suspended_unlocked(unsigned long timeoutMs) noexcept
    {
        if (!SleepConditionVariableSRW(CONDVAR(handle), (PSRWLOCK)cs.native_handle(), timeoutMs, 0))
        {
            if (GetLastError() == ERROR_TIMEOUT)
                return std::cv_status::timeout;
        }
        return std::cv_status::no_timeout;
    }

    // NOTE: the granularity of WinAPI SleepConditionVariable is ~15.6ms
    //       which is the default scheduling rate on Windows systems, and windows is not a realtime OS
    //       This method attempts to use a multimedia timer to achieve higher precision sleeps
    std::cv_status condition_variable::_wait_for_unlocked(rpp::int64 wait_nanos) noexcept
    {
        // BUGFIX: We can only do ONE SleepConditionVariableSRW call in this function
        //         because the mutex is unlocked and re-locked by SleepConditionVariableSRW
        //         which can cause a race condition and a deadlock to occur
        if (wait_nanos <= 0)
        {
            if (SleepConditionVariableSRW(CONDVAR(handle), (PSRWLOCK)cs.native_handle(), 0, 0))
                return std::cv_status::no_timeout;
            return std::cv_status::timeout;
        }

        MMRESULT mmStatus = timeBeginPeriod(1);

        DWORD wait_ms = static_cast<DWORD>(wait_nanos / 1'000'000);
        if (wait_ms == 0) wait_ms = 1;

        std::cv_status status = std::cv_status::no_timeout;
        if (!SleepConditionVariableSRW(CONDVAR(handle), (PSRWLOCK)cs.native_handle(), wait_ms, 0))
        {
            if (GetLastError() == ERROR_TIMEOUT)
                status = std::cv_status::timeout;
        }

        if (mmStatus == TIMERR_NOERROR)
        {
            timeEndPeriod(1);
        }

        return status;
    }

}

#endif // _MSC_VER

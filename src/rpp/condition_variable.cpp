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
#if USE_RPP_CONDITION_VARIABLE
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#include <realtimeapiset.h>
#include <timeapi.h>
#pragma comment(lib, "Winmm.lib") // MM time library
#pragma comment(lib, "Mincore.lib")

namespace rpp
{
    // NOTE on debugging: enabling this will cause extra delays in the timing logic,
    //                    which will show false positive test failures; always disable this before testing
    #define DEBUG_CVAR 0
    #define USE_SRWLOCK 0
    #define USE_WAITABLE_EVENTS 0

    #if DEBUG_CVAR
        #define IN_CVAR_DEBUG(expr) expr
    #else
        #define IN_CVAR_DEBUG(...)
    #endif

    #if !USE_SRWLOCK
        #define IN_CSLOCK(expr) expr
        #define IN_SRWLOCK(...)
    #else
        #define IN_CSLOCK(...)
        #define IN_SRWLOCK(expr) expr
    #endif

    struct condition_variable::mutex_type final
        IN_CSLOCK(: CRITICAL_SECTION)
        IN_SRWLOCK(: SRWLOCK)
    {
        mutex_type() noexcept {
            IN_CSLOCK((void)InitializeCriticalSectionAndSpinCount(this, 8000));
            IN_SRWLOCK(InitializeSRWLock(this));
        }
        ~mutex_type() noexcept {
            IN_CSLOCK(DeleteCriticalSection(this));
        }
        void lock() noexcept {
            IN_CSLOCK(EnterCriticalSection(this));
            IN_SRWLOCK(AcquireSRWLockExclusive(this));
        }
        void unlock() noexcept {
            IN_CSLOCK(LeaveCriticalSection(this));
            IN_SRWLOCK(ReleaseSRWLockExclusive(this));
        }
    };
    
    #define CONDVAR(handle) PCONDITION_VARIABLE(&handle)

    condition_variable::condition_variable() noexcept
    {
        mtx = std::make_shared<mutex_type>();
    #if USE_WAITABLE_EVENTS
        handle.impl = CreateEventA(nullptr, /*manual reset*/false, /*initial state*/false, nullptr);
    #else
        // Win32 COND VARS are allocated once and never deleted
        InitializeConditionVariable(CONDVAR(handle));
    #endif
    }

    condition_variable::~condition_variable() noexcept
    {
    #if USE_WAITABLE_EVENTS
        if (handle.impl)
        {
            CloseHandle(handle.impl);
            handle.impl = nullptr;
        }
    #endif
        if (timer)
        {
            CloseHandle(timer);
            timer = nullptr;
        }
    }

    void condition_variable::notify_one() noexcept
    {
    #if USE_WAITABLE_EVENTS
        SetEvent(handle.impl);
    #else
        WakeConditionVariable(CONDVAR(handle));
    #endif
    }

    void condition_variable::notify_all() noexcept
    {
    #if USE_WAITABLE_EVENTS
        SetEvent(handle.impl);
    #else
        WakeAllConditionVariable(CONDVAR(handle));
    #endif
    }

    void condition_variable::_lock(mutex_type* m) noexcept
    {
        m->lock();
    }

    void condition_variable::_unlock(mutex_type* m) noexcept
    {
        m->unlock();
    }

    bool condition_variable::_is_signaled(mutex_type* m) noexcept
    {
        // gets current state and resets the event
    #if USE_WAITABLE_EVENTS
        return WaitForSingleObject(handle.impl, 0) == WAIT_OBJECT_0;
    #else
        BOOL result = IN_CSLOCK(SleepConditionVariableCS(CONDVAR(handle), m, 0))
                      IN_SRWLOCK(SleepConditionVariableSRW(CONDVAR(handle), m, 0, 0));
        return result;
    #endif
    }

    std::cv_status condition_variable::_wait_suspended_unlocked(mutex_type* m, unsigned long timeoutMs) noexcept
    {
    #if USE_WAITABLE_EVENTS
        m->unlock();
        std::cv_status status = std::cv_status::timeout;
        if (WaitForSingleObject(handle.impl, timeoutMs) == WAIT_OBJECT_0)
            status = std::cv_status::no_timeout;
        m->lock();
        return status;
    #else
        BOOL result = IN_CSLOCK(SleepConditionVariableCS(CONDVAR(handle), m, timeoutMs))
                      IN_SRWLOCK(SleepConditionVariableSRW(CONDVAR(handle), m, timeoutMs, 0));
        if (!result)
        {
            // timeout or a hard failure
            DWORD err = GetLastError();
            if (err == ERROR_TIMEOUT)
                return std::cv_status::timeout;
        }
        return std::cv_status::no_timeout;
    #endif
    }

    // NOTE: the granularity of WinAPI SleepConditionVariable is ~15.6ms
    //       which is the default scheduling rate on Windows systems, and windows is not a realtime OS
    //       This method attempts to use a multimedia timer to achieve higher precision sleeps
    std::cv_status condition_variable::_wait_for_unlocked(mutex_type* m, const duration& rel_time) noexcept
    {
        // -- mutex m is acquired --
        if (_is_signaled(m))
            return std::cv_status::no_timeout;

        const int64_t wait_nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(rel_time).count();
        if (wait_nanos <= 0)
            return std::cv_status::timeout;

        MMRESULT mmStatus = timeBeginPeriod(1);

    #if USE_WAITABLE_EVENTS
        if (!timer) timer = CreateWaitableTimer(nullptr, /*manual reset*/true, nullptr);

        LARGE_INTEGER dueTime;
        dueTime.QuadPart = -(wait_nanos / 100); // negative means relative time
        SetWaitableTimer(timer, &dueTime, 0, nullptr, nullptr, 0);

        // the order is important here, if both events are signaled, the lowest index is returned
        HANDLE handles[2] = { handle.impl, timer  };
        std::cv_status status = std::cv_status::timeout;

        int wait_ms = static_cast<int>(wait_nanos / 1'000'000);
        m->unlock();
        int r = WaitForMultipleObjects(2, handles, /*wait all*/false, wait_ms);
        m->lock();

        if (r == WAIT_OBJECT_0) // event signaled 
        {
            status = std::cv_status::no_timeout;
            IN_CVAR_DEBUG(printf("condvar: event signaled\n"));
        }
        else if (r == WAIT_OBJECT_0 + 1) // timeout timer signaled, check the state
        {
            status = std::cv_status::timeout;
            IN_CVAR_DEBUG(printf("condvar: timeout timer, status=%d\n", status));
        }
    #else
        int wait_ms = static_cast<int>(wait_nanos / 1'000'000);
        if (wait_ms <= 0) wait_ms = 1;
        std::cv_status status = _wait_suspended_unlocked(m, static_cast<DWORD>(wait_ms));
    #endif

        if (mmStatus == TIMERR_NOERROR)
        {
            timeEndPeriod(1);
        }

        return status;
    }

}

#endif // _MSC_VER

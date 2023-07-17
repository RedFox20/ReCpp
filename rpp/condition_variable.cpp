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

namespace rpp
{
    // NOTE on debugging: enabling this will cause extra delays in the timing logic,
    //                    which will show false positive test failures; always disable this before testing
    #define DEBUG_CVAR 0
    #define USE_SRWLOCK 0

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
        // Win32 COND VARS are allocated once and never deleted
        InitializeConditionVariable(CONDVAR(handle));
    }

    condition_variable::~condition_variable() noexcept = default;

    void condition_variable::notify_one() noexcept
    {
        WakeConditionVariable(CONDVAR(handle));
    }

    void condition_variable::notify_all() noexcept
    {
        WakeAllConditionVariable(CONDVAR(handle));
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
        BOOL result = IN_CSLOCK(SleepConditionVariableCS(CONDVAR(handle), m, 0))
                      IN_SRWLOCK(SleepConditionVariableSRW(CONDVAR(handle), m, 0, 0));
        return result;
    }

    std::cv_status condition_variable::_wait_suspended_unlocked(mutex_type* m, unsigned long timeoutMs) noexcept
    {
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
    }

    IN_CVAR_DEBUG(inline double to_ms(const condition_variable::duration& dur) noexcept
                  { return std::chrono::duration<double>{dur}.count() * 1000; })

    // NOTE: the granularity of WinAPI SleepConditionVariable is ~15.6ms
    //       which is the default scheduling rate on Windows systems, and windows is not a realtime OS
    std::cv_status condition_variable::_wait_for_unlocked(mutex_type* m, const duration& rel_time) noexcept
    {
        std::cv_status status = std::cv_status::timeout;

        // for long waits, we first do a long suspended sleep, minus the windows timer tick rate
        constexpr duration suspendThreshold = std::chrono::milliseconds{16};
        constexpr duration periodThreshold = std::chrono::milliseconds{2};
        constexpr duration zero = duration{0};

        duration dur = rel_time;
        time_point endTime = clock::now() + dur;

        if (dur > suspendThreshold)
        {
            // we need to measure exact suspend start time in case we timed out
            // because the suspension timeout is not accurate at all
            time_point suspendStart = clock::now();

            int64_t timeout = std::chrono::duration_cast<std::chrono::milliseconds>(dur - suspendThreshold).count();
            if (timeout < 0) timeout = 0;

            status = _wait_suspended_unlocked(m, static_cast<DWORD>(timeout));
            if (status == std::cv_status::timeout)
                dur = endTime - clock::now();

            IN_CVAR_DEBUG(duration totalElapsed = (clock::now() - suspendStart);
                          printf("suspended wait %p dur=%.2fms elapsed: %.2fms %s\n",
                                  CONDVAR(handle), to_ms(rel_time), to_ms(totalElapsed),
                                  status == std::cv_status::no_timeout ? "SIGNALED" : "timeout"));
            if (dur <= zero)
                return status;
        }

        // spin loop is required because timeout is too short, or suspended wait timed out
        if (status == std::cv_status::timeout)
        {
            duration remaining = dur;
            IN_CVAR_DEBUG(time_point spinStart = clock::now());

            // always enter the loop at least once, in case this is a 0-timeout condition check
            do
            {
                if (_is_signaled(m))
                {
                    status = std::cv_status::no_timeout;
                    break;
                }

                // on low-cpu count systems, SleepEx can lock us out for an indefinite period,
                // so we're using yield for precision sleep here
                YieldProcessor();

                remaining = endTime - clock::now();
            }
            while (remaining > zero);

            IN_CVAR_DEBUG(duration totalElapsed = (clock::now() - spinStart);
                          printf("spinwait %p spin=%.2fms elapsed: %.2fms %s\n",
                                  CONDVAR(handle), to_ms(dur), to_ms(totalElapsed),
                                  status == std::cv_status::no_timeout ? "SIGNALED" : "timeout"));
        }
        return status;
    }

}

#endif // _MSC_VER

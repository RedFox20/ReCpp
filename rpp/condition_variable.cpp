#include "condition_variable.h"

#if _MSC_VER
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#endif

namespace rpp
{
#if _MSC_VER

    #define CONDVAR(handle) PCONDITION_VARIABLE(&handle)
    #define USE_SRWLOCK 0

    struct condition_variable::mutex_type
        #if !USE_SRWLOCK
            : CRITICAL_SECTION
        #else
            : SRWLOCK
        #endif
    {
    };

    condition_variable::condition_variable() noexcept
    {
        mtx = std::make_shared<mutex_type>();
        #if !USE_SRWLOCK
            //InitializeCriticalSection(mtx.get());
            (void)InitializeCriticalSectionAndSpinCount(mtx.get(), 8000);
        #else
            InitializeSRWLock(mtx.get());
        #endif

        // Win32 COND VARS are allocated once and never deleted
        InitializeConditionVariable(CONDVAR(handle));
    }

    condition_variable::~condition_variable() noexcept
    {
        #if !USE_SRWLOCK
            DeleteCriticalSection(mtx.get());
        #endif
    }

    void condition_variable::notify_one() noexcept
    {
        WakeConditionVariable(CONDVAR(handle));
    }

    void condition_variable::notify_all() noexcept
    {
        WakeAllConditionVariable(CONDVAR(handle));
    }

    struct unlock_guard {
        std::unique_lock<std::mutex>& mtx;
        explicit unlock_guard(std::unique_lock<std::mutex>& mtx) : mtx{mtx} {
            mtx.unlock();
        }
        ~unlock_guard() noexcept /* terminates */ {
            // relock mutex or terminate()
            // condition_variable_any wait functions are required to terminate if
            // the mutex cannot be relocked;
            // we slam into noexcept here for easier user debugging.
            mtx.lock();
        }
    };

    void condition_variable::wait(std::unique_lock<std::mutex>& lock) noexcept
    {
        std::shared_ptr<mutex_type> m = mtx; // for immunity to *this destruction
        #if !USE_SRWLOCK
            EnterCriticalSection(m.get());
            unlock_guard unlockOuter{lock}; // unlock the outer lock, and relock when we exit the scope
            SleepConditionVariableCS(CONDVAR(handle), m.get(), INFINITE);
            LeaveCriticalSection(m.get());
        #else
            AcquireSRWLockExclusive(m.get());
            unlock_guard unlockOuter{lock}; // unlock the outer lock, and relock when we exit the scope
            SleepConditionVariableSRW(CONDVAR(handle), m.get(), INFINITE, 0);
            ReleaseSRWLockExclusive(m.get());
        #endif
    }
    
    // NOTE: both SleepConditionVariable* have a granularity of ~15ms for the timeout
    //       that's because some kind of SLEEP() command is used, and windows is not a realtime OS
    //       The solution here is to pass in rel_time=0, which allows the condition variable to be checked
    std::cv_status condition_variable::wait_for(std::unique_lock<std::mutex>& lock, 
                                                const duration& rel_time) noexcept
    {
        std::cv_status status = std::cv_status::no_timeout;
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(rel_time);
        int64_t timeoutMs = millis.count();
        if (timeoutMs <= 0) timeoutMs = 0;

        std::shared_ptr<mutex_type> m = mtx; // for immunity to *this destruction

        #if !USE_SRWLOCK
            EnterCriticalSection(m.get());
            unlock_guard unlockOuter{lock}; // unlock the outer lock, and relock when we exit the scope
            if (!SleepConditionVariableCS(CONDVAR(handle), m.get(), static_cast<DWORD>(timeoutMs)))
            {
                // timeout or a hard failure
                DWORD err = GetLastError();
                if (err == ERROR_TIMEOUT)
                    status = std::cv_status::timeout;
            }
            else
            {
                if (timeoutMs == 0)
                    status = std::cv_status::no_timeout;
            }
            LeaveCriticalSection(m.get());
        #else
            AcquireSRWLockExclusive(m.get());
            unlock_guard unlockOuter{lock}; // unlock the outer lock, and relock when we exit the scope
            if (!SleepConditionVariableSRW(CONDVAR(handle), m.get(), static_cast<DWORD>(timeoutMs), 0))
            {
                // timeout or a hard failure
                if (GetLastError() == ERROR_TIMEOUT)
                    status = std::cv_status::timeout;
            }
            ReleaseSRWLockExclusive(m.get());
        #endif

        return status;
    }

    std::cv_status condition_variable::wait_until(std::unique_lock<std::mutex>& lock, 
                                                  const time_point& abs_time) noexcept
    {
        // in the actual WinAPI level, the solution is to use rel_time,
        duration rel_time = (abs_time - clock::now());
        return wait_for(lock, rel_time);
    }

#endif
}
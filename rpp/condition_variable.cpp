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
    #define USE_EVENT 0

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
    
    struct unlock_guard
    {
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

    condition_variable::condition_variable() noexcept
    {
        mtx = std::make_shared<mutex_type>();
        #if !USE_EVENT
            // Win32 COND VARS are allocated once and never deleted
            InitializeConditionVariable(CONDVAR(handle));
        #endif
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

    void condition_variable::wait(std::unique_lock<std::mutex>& lock) noexcept
    {
        std::shared_ptr<mutex_type> m = mtx; // for immunity to *this destruction

        std::unique_lock csGuard {*m}; // critical section
        unlock_guard unlockOuter {lock}; // unlock the outer lock, and relock when we exit the scope
        _wait_suspended_unlocked(m.get(), INFINITE);
        csGuard.unlock(); // unlock critical section before relocking outer
    }

    constexpr DWORD clampTimeout(int64_t timeoutMs) noexcept
    {
        return timeoutMs <= 0 ? 0 : static_cast<DWORD>(timeoutMs);
    }
    constexpr DWORD getTimeoutMs(const condition_variable::duration& dur) noexcept
    {
        return clampTimeout(duration_cast<std::chrono::milliseconds>(dur).count());
    }
    
    // NOTE: the granularity of WinAPI SleepConditionVariable is ~15.6ms
    //       which is the default scheduling rate on Windows systems, and windows is not a realtime OS
    std::cv_status condition_variable::wait_for(std::unique_lock<std::mutex>& lock, 
                                                const duration& rel_time) noexcept
    {
        std::cv_status status = std::cv_status::timeout;

        using namespace std::chrono_literals;
        std::shared_ptr<mutex_type> cs = mtx; // for immunity to *this destruction
        
        std::unique_lock csGuard {*cs}; // critical section guard
        unlock_guard unlockOuter {lock}; // unlock the outer lock, and relock when we exit the scope

        duration spinDuration;

        // for long waits, we first do a long suspended sleep,
        // but with granularity that aligns to windows timer tick rate
        if (rel_time > 16ms)
        {
            spinDuration = 16ms;
            DWORD timeout1 = getTimeoutMs(rel_time - 16ms);
            time_point start = clock::now();
            status = _wait_suspended_unlocked(cs.get(), timeout1);

            std::chrono::duration<double> elapsed = (clock::now() - start);
            printf("suspended wait %p elapsed: %.2fms %s\n", CONDVAR(handle), elapsed.count()*1000,
                    status == std::cv_status::no_timeout ? "no_timeout" : "timeout");
        }
        else // for short waits we have to spin loop until the time is out
        {
            spinDuration = rel_time;
        }

        // spin loop is required because timeout is too short, or suspended wait timed out
        if (status == std::cv_status::timeout)
        {
            time_point start = clock::now();
            time_point now = start;
            time_point end = now + spinDuration;
            do
            {
                if (_is_signaled(cs.get()))
                {
                    status = std::cv_status::no_timeout;

                    std::chrono::duration<double> elapsed = (clock::now() - start);
                    printf("spinwait %p elapsed: %.2fms SIGNALED\n", CONDVAR(handle), elapsed.count()*1000);
                    break;
                }

                Sleep(0); // sleep 0 is special, gives priority to other threads

                //std::chrono::duration<double> elapsed = (clock::now() - now);
                //printf("spinwait elapsed: %.2fms\n", elapsed.count()*1000);

                now = clock::now();
            }
            while (now < end);
        }

        csGuard.unlock(); // unlock critical section before relocking outer
        return status;
    }

    std::cv_status condition_variable::wait_until(std::unique_lock<std::mutex>& lock, 
                                                  const time_point& abs_time) noexcept
    {
        // in the actual WinAPI level, the solution is to use rel_time,
        duration rel_time = (abs_time - clock::now());
        return wait_for(lock, rel_time);
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
        else
        {
        }
        return std::cv_status::no_timeout;
    }

    bool condition_variable::_is_signaled(mutex_type* m) noexcept
    {
        BOOL result = IN_CSLOCK(SleepConditionVariableCS(CONDVAR(handle), m, 0))
                      IN_SRWLOCK(SleepConditionVariableSRW(CONDVAR(handle), m, 0, 0));
        return result;
    }

#endif
}
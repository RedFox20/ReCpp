#include "mutex.h"
#if _MSC_VER && USE_CUSTOM_WINDOWS_MUTEX
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif

namespace rpp
{
#if _MSC_VER && USE_CUSTOM_WINDOWS_MUTEX
    /////////////////////////////////////////////////////////////////

    #define GET_SRW_LOCK() ((SRWLOCK*)&mtx)

    mutex::mutex() noexcept : mtx{SRWLOCK_INIT}
    {
        static_assert(sizeof(SRWLOCK) == sizeof(mutex::mtx));
    }
    mutex::~mutex() noexcept
    {
        mtx = SRWLOCK_INIT;
    }
    bool mutex::try_lock() noexcept
    {
        SRWLOCK* srw = GET_SRW_LOCK();
        return srw && TryAcquireSRWLockExclusive(srw);
    }
    void mutex::lock()
    {
        if (SRWLOCK* srw = GET_SRW_LOCK())
            AcquireSRWLockExclusive(srw);
        else
            throw std::runtime_error{"rpp::mutex::lock() failed: lock destroyed"};
    }
    void mutex::unlock() noexcept
    {
        if (SRWLOCK* srw = GET_SRW_LOCK())
            ReleaseSRWLockExclusive(srw);
    }

    /////////////////////////////////////////////////////////////////

    #define GET_RMUTEX() ((CRITICAL_SECTION*)mtx)

    recursive_mutex::recursive_mutex() noexcept
    {
        mtx = new CRITICAL_SECTION;
        // Docs: After a thread has ownership of a critical section,
        //       it can make additional calls to EnterCriticalSection 
        //       or TryEnterCriticalSection without blocking its execution. 
        InitializeCriticalSection(GET_RMUTEX());
    }
    recursive_mutex::~recursive_mutex() noexcept
    {
        if (CRITICAL_SECTION* cs = GET_RMUTEX())
        {
            mtx = nullptr;
            DeleteCriticalSection(cs);
            delete cs;
        }
    }
    bool recursive_mutex::try_lock() noexcept
    {
        CRITICAL_SECTION* cs = GET_RMUTEX();
        return cs && TryEnterCriticalSection(cs);
    }
    void recursive_mutex::lock()
    {
        if (CRITICAL_SECTION* cs = GET_RMUTEX())
            EnterCriticalSection(cs);
        else
            throw std::runtime_error{"rpp::recursive_mutex::lock() failed: lock destroyed"};
    }
    void recursive_mutex::unlock() noexcept
    {
        if (CRITICAL_SECTION* cs = GET_RMUTEX())
            LeaveCriticalSection(cs);
    }
#endif
}

#include "mutex.h"
#if _MSC_VER && USE_CUSTOM_WINDOWS_MUTEX
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif
#if RPP_FREERTOS
    #include "debugging.h"
    #include <FreeRTOS.h>
    #include <semphr.h>
    #include <task.h>
#endif
#if RPP_CORTEX_M_ARCH
    #include RPP_CORTEX_M_DEVICE_H
    #include RPP_CORTEX_M_CORE_H
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
#elif RPP_FREERTOS
    /////////////////////////////////////////////////////////////////
    #define GET_SEMPHR() ((SemaphoreHandle_t)ctx)

    mutex::mutex() noexcept
    {
        ctx = xSemaphoreCreateBinary(); // binary semaphore is faster than mutex semaphore
        xSemaphoreGive(GET_SEMPHR());   // Initialize to available
    }

    mutex::~mutex() noexcept
    {
        if (ctx)
        {
            vSemaphoreDelete(GET_SEMPHR());
            ctx = nullptr;
        }
    }

    bool mutex::try_lock() noexcept
    { 
        if (xPortIsInsideInterrupt())
        {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            bool r = xSemaphoreTakeFromISR(GET_SEMPHR(), &higherPriorityTaskWoken) == pdTRUE;
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
            return r;
        }

        return xSemaphoreTake(GET_SEMPHR(), 0) == pdTRUE;
    }
    
    void mutex::lock() noexcept
    {
        if (xPortIsInsideInterrupt())
        {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xSemaphoreTakeFromISR(GET_SEMPHR(), &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }
        else
        {
            xSemaphoreTake(GET_SEMPHR(), portMAX_DELAY);
        }
    }
    
    void mutex::unlock() noexcept
    {
        if (xPortIsInsideInterrupt())
        {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(GET_SEMPHR(), &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }
        else
        {
            xSemaphoreGive(GET_SEMPHR());
        }
    }

    /////////////////////////////////////////////////////////////////

#if (configUSE_RECURSIVE_MUTEXES == 1)
    recursive_mutex::recursive_mutex() noexcept
    {
        ctx = xSemaphoreCreateRecursiveMutex(); // Recursive semaphore
        xSemaphoreGiveRecursive(GET_SEMPHR());  // Initialize to available
    }

    recursive_mutex::~recursive_mutex() noexcept
    {
        if (ctx)
        {
            vSemaphoreDelete(GET_SEMPHR());
            ctx = nullptr;
        }
    }

    bool recursive_mutex::try_lock() noexcept
    {
        Assert(xPortIsInsideInterrupt(), "Cannot take recursive mutex from ISR");
        return xSemaphoreTakeRecursive(GET_SEMPHR(), 0) == pdTRUE;
    }

    void recursive_mutex::lock() noexcept
    {
        Assert(xPortIsInsideInterrupt(), "Cannot take recursive mutex from ISR");
        xSemaphoreTakeRecursive(GET_SEMPHR(), portMAX_DELAY);
    }

    void recursive_mutex::unlock() noexcept
    {
        Assert(xPortIsInsideInterrupt(), "Cannot give recursive mutex from ISR");
        xSemaphoreGiveRecursive(GET_SEMPHR());
    }
#endif // configUSE_RECURSIVE_MUTEXES

    /////////////////////////////////////////////////////////////////

    bool critical_section::try_lock() noexcept
    {
        // When called from interrupt
        if (xPortIsInsideInterrupt())
            saved_interrupt_state = taskENTER_CRITICAL_FROM_ISR();
        
        // When called from task
        else
            portENTER_CRITICAL();
        return true;
    }

    void critical_section::lock() noexcept
    {
        try_lock();
    }

    void critical_section::unlock() noexcept
    {
        // When called from interrupt
        if (xPortIsInsideInterrupt())
            taskEXIT_CRITICAL_FROM_ISR(saved_interrupt_state);

        // When called from task
        else
            portEXIT_CRITICAL();
    }
#elif RPP_CORTEX_M_ARCH
    /////////////////////////////////////////////////////////////////

    bool critical_section::try_lock() noexcept
    {
        lock();
        return true;
    }

    void critical_section::lock() noexcept
    {
        // Save current interrupt state
        uint32_t primask = __get_PRIMASK();

        // Disable interrupts
        __disable_irq();

        if (mtx.locked++ == 0)
            mtx.primask = primask;
    }

    void critical_section::unlock() noexcept
    {
        if (mtx.locked > 0 && --mtx.locked == 0)
        {
            // Restore previous interrupt state
            __set_PRIMASK(mtx.primask);
        }
    }
#endif
}

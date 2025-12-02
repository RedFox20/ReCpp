#include "mutex.h"
#if _MSC_VER && USE_CUSTOM_WINDOWS_MUTEX
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif
#if RPP_FREERTOS
    #include "FreeRTOS.h"
    #include "semphr.h"
    #include "task.h"
#endif
#if RPP_STM32_HAL
    #include RPP_STM32_CORE_H
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

    mutex::mutex()
    {
        mtx.ctx = xSemaphoreCreateBinary(); // binary semaphore is faster than mutex semaphore
        xSemaphoreGive((SemaphoreHandle_t)mtx.ctx); // Initialize to available
    }

    mutex::~mutex()
    {
        if (mtx.ctx)
        {
            vSemaphoreDelete((SemaphoreHandle_t)mtx.ctx);
            mtx.ctx = nullptr;
        }
    }

    bool mutex::try_lock()
    { 
        if (xPortIsInsideInterrupt())
        {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            bool r = xSemaphoreTakeFromISR((SemaphoreHandle_t)mtx.ctx, &higherPriorityTaskWoken) == pdTRUE;
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
            return r;
        }

        return xSemaphoreTake((SemaphoreHandle_t)mtx.ctx, 0) == pdTRUE;
    }
    
    void mutex::lock()
    {
        if (xPortIsInsideInterrupt())
        {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xSemaphoreTakeFromISR((SemaphoreHandle_t)mtx.ctx, &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }

        xSemaphoreTake((SemaphoreHandle_t)mtx.ctx, portMAX_DELAY);
    }
    
    void mutex::unlock()
    {
        if (xPortIsInsideInterrupt())
        {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR((SemaphoreHandle_t)mtx.ctx, &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }

        xSemaphoreGive((SemaphoreHandle_t)mtx.ctx);
    }

    /////////////////////////////////////////////////////////////////

    recursive_mutex::recursive_mutex()
    {
        mtx.ctx = xSemaphoreCreateRecursiveMutex(); // Recursive semaphore
        xSemaphoreGiveRecursive((SemaphoreHandle_t)mtx.ctx);      // Initialize to available
    }

    recursive_mutex::~recursive_mutex()
    {
        if (mtx.ctx)
        {
            vSemaphoreDelete((SemaphoreHandle_t)mtx.ctx);
            mtx.ctx = nullptr;
        }
    }

    bool recursive_mutex::try_lock()
    {
        if (xPortIsInsideInterrupt())
        {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            bool r = xSemaphoreTakeRecursiveFromISR((SemaphoreHandle_t)mtx.ctx, &higherPriorityTaskWoken) == pdTRUE;
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
            return r;
        }

        return xSemaphoreTakeRecursive((SemaphoreHandle_t)mtx.ctx, 0) == pdTRUE;
    }

    void recursive_mutex::lock()
    {
        if (xPortIsInsideInterrupt())
        {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xSemaphoreTakeRecursiveFromISR((SemaphoreHandle_t)mtx.ctx, &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }

        xSemaphoreTakeRecursive((SemaphoreHandle_t)mtx.ctx, portMAX_DELAY);
    }

    void recursive_mutex::unlock()
    {
        if (xPortIsInsideInterrupt())
        {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveRecursiveFromISR((SemaphoreHandle_t)mtx.ctx, &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }

        xSemaphoreGiveRecursive((SemaphoreHandle_t)mtx.ctx);
    }

    /////////////////////////////////////////////////////////////////

    bool critical_section::try_lock()
    {
        portENTER_CRITICAL(0);
        return true;
    }

    void critical_section::lock()
    {
        portENTER_CRITICAL(0);
    }

    void critical_section::unlock()
    {
        portEXIT_CRITICAL(0);
    }
#elif RPP_STM32_HAL
    /////////////////////////////////////////////////////////////////

    bool critical_section::try_lock()
    {
        lock();
        return true;
    }

    void critical_section::lock()
    {
        if (mtx.locked++ == 0)
            mtx.primask = __get_PRIMASK();
        __disable_irq();
    }

    void critical_section::unlock()
    {
        if (mtx.locked > 0 && --mtx.locked == 0)
        {
            __set_PRIMASK(mtx.primask);
        }
    }
#endif
}

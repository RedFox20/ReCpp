#pragma once
/// FreeRTOS for the rpp tests on qemu-system-arm -M mps2-an386, a Cortex-M4F

#define configCPU_CLOCK_HZ                   25000000UL
#define configTICK_RATE_HZ                   1000
#define configUSE_PREEMPTION                 1
#define configUSE_IDLE_HOOK                  0
#define configUSE_TICK_HOOK                  0
#define configMAX_PRIORITIES                 5
#define configMINIMAL_STACK_SIZE             256
#define configMAX_TASK_NAME_LEN              16
#define configTICK_TYPE_WIDTH_IN_BITS        TICK_TYPE_WIDTH_32_BITS
#define configUSE_MUTEXES                    1
#define configUSE_RECURSIVE_MUTEXES          1
#define configUSE_COUNTING_SEMAPHORES        1
#define configSUPPORT_DYNAMIC_ALLOCATION     1
#define configSUPPORT_STATIC_ALLOCATION      0
#define configUSE_TIMERS                     0
#define configCHECK_FOR_STACK_OVERFLOW       2
#define configUSE_MALLOC_FAILED_HOOK         1

// no configPRIO_BITS: the port asserts it against the hardware, and QEMU implements every priority bit
#define configKERNEL_INTERRUPT_PRIORITY      255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 191

#define INCLUDE_vTaskDelay                   1
#define INCLUDE_vTaskDelete                  1
#define INCLUDE_xTaskGetCurrentTaskHandle    1
#define INCLUDE_uxTaskPriorityGet            1

#ifdef __cplusplus
extern "C"
#endif
void rpp_cm_assert_failed(const char* file, int line);
#define configASSERT(x) if (!(x)) rpp_cm_assert_failed(__FILE__, __LINE__)

// the port handlers take the CMSIS names, so startup.c needs no wrappers
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

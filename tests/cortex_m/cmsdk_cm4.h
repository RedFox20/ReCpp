#pragma once
/// The CMSIS device header of the Cortex-M4 FPGA image which qemu-system-arm -M mps2-an386 models
typedef enum IRQn
{
    NonMaskableInt_IRQn   = -14,
    HardFault_IRQn        = -13,
    MemoryManagement_IRQn = -12,
    BusFault_IRQn         = -11,
    UsageFault_IRQn       = -10,
    SVCall_IRQn           = -5,
    DebugMonitor_IRQn     = -4,
    PendSV_IRQn           = -2,
    SysTick_IRQn          = -1,
} IRQn_Type;

#define __CM4_REV              0x0001U
#define __MPU_PRESENT          1U
#define __NVIC_PRIO_BITS       8U // QEMU implements every priority bit, which the FPGA image does not
#define __Vendor_SysTickConfig 0U
#define __FPU_PRESENT          1U

#include <core_cm4.h>

extern uint32_t SystemCoreClock; // the CPU clock in Hz

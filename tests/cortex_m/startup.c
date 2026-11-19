// The vector table and the reset handler for qemu-system-arm -M mps2-an386
#include "cmsdk_cm4.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // _exit

extern uint32_t __stack;
void _start(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

void Reset_Handler(void)
{
    SCB->CPACR |= 0xFU << 20; // -mfloat-abi=hard needs CP10 and CP11 before the first FPU instruction
    __DSB();
    __ISB();
    _start(); // the newlib rdimon crt0 clears .bss, runs the constructors and calls exit(main())
}

// newlib has no TLS on Cortex-M. The ARM TLS ABI puts the block after a TCB at the thread pointer.
// A caller expects r1 to r3 unchanged, so no C body may compute it
__attribute__((naked)) void* __aeabi_read_tp(void)
{
    __asm volatile("ldr r0, =__tls_start - 8\n bx lr");
}

// a fault exits QEMU with an error, so the CI job fails at once and does not wait for its timeout
static void Fault_Handler(void)
{
    printf("Cortex-M fault: CFSR=0x%08lx HFSR=0x%08lx\n", (unsigned long)SCB->CFSR, (unsigned long)SCB->HFSR);
    _exit(99);
}

__attribute__((section(".isr_vector"), used))
void (* const vectors[16])(void) =
{
    (void (*)(void))&__stack, Reset_Handler,
    Fault_Handler, Fault_Handler, Fault_Handler, Fault_Handler, Fault_Handler,
    0, 0, 0, 0,
    SVC_Handler, Fault_Handler, 0, PendSV_Handler, SysTick_Handler,
};

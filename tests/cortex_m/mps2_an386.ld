/* QEMU loads each section at its address, so .data needs no copy from flash */
MEMORY
{
    CODE (rwx) : ORIGIN = 0x00000000, LENGTH = 4M
    RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}

ENTRY(Reset_Handler)

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > CODE
    .text : { *(.text*) KEEP(*(.init)) KEEP(*(.fini)) *(.rodata*) } > CODE
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > CODE
    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > CODE
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > CODE
    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > CODE
    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > CODE
    .data : { *(.data*) } > RAM
    /* one TLS block for every task, which __aeabi_read_tp in startup.c returns */
    .tdata : ALIGN(8)
    {
        __tls_start = .;
        *(.tdata .tdata.*)
        *(.tbss .tbss.*)
    } > RAM
    /* ld gives .tbss no address space, so .bss starts after the TLS block, and does not overlap it */
    .bss ALIGN(ADDR(.tdata) + SIZEOF(.tdata), 8) :
    {
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        __bss_end__ = .;
    } > RAM
    /* the heap of _sbrk in startup.c, below the main stack */
    end = .;
    __end__ = .;
    __stack = ORIGIN(RAM) + LENGTH(RAM);
    __heap_end = __stack - 64K;
}

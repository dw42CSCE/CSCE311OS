/* entry.S - set up stack, call main() */
    .section .text
    .global _start
    .global kernel_main

_start:
    la sp, _stack_top   /* load stack pointer */
    call kernel_main
    /* loop forever if main returns */
1:  j 1b

/* space for stack, defined in linker script as _stack_top */

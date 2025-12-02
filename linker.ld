/* linker.ld */
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
  . = 0x80200000;

  .text : {
    *(.text*)
  }

  .rodata : { *(.rodata*) }

  .data : { *(.data*) }

  .bss : {
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    __bss_end = .;
  }

  /* place the initial stack after all data/bss to avoid clobbering globals */
  . = ALIGN(16);
  PROVIDE(_stack_top = __bss_end + 0x4000); /* 16 KiB stack grows downward */
}

/* main.c - tiny kernel: write hello to UART (virt platform at 0x10000000) */
typedef unsigned long u64;

/* Avoid making a global pointer (which ends up in .sdata and causes HI20 reloc issues) */
#define UART_ADDR 0x10000000UL
#define UART ((volatile unsigned char*)UART_ADDR)

static inline void uart_putc(char c) {
    *UART = (unsigned char)c;
}

/* simple puts */
static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

/* very small main */
void main(void) {
    uart_puts("My os: Kernel works\n");
    for (;;) { }
}

#include "uart.h"
#include <stdint.h>

#define UART0 0x10000000UL   // base address for UART

static inline void mmio_write(uintptr_t addr, uint8_t v) {
    *(volatile uint8_t*)addr = v;
}

static inline uint8_t mmio_read(uintptr_t addr) {
    return *(volatile uint8_t*)addr;
}

void uart_putc(char c) {
    // Wait for THR empty (LSR bit 5)
    while (!(mmio_read(UART0 + 5) & 0x20)) {}

    if (c == '\n') {
        mmio_write(UART0 + 0, '\r');
        while (!(mmio_read(UART0 + 5) & 0x20)) {}
        mmio_write(UART0 + 0, '\n');
        return;
    }

    mmio_write(UART0 + 0, (uint8_t)c);
}

int uart_haschar(void) {
    // LSR bit 0 = Data Ready
    return (mmio_read(UART0 + 5) & 1);
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

int uart_getc(void) {
    // Wait for data ready (LSR bit 0)
    while (!(mmio_read(UART0 + 5) & 1)) {}
    return (int)mmio_read(UART0 + 0);
}

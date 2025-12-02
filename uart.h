#ifndef UART_H
#define UART_H
#include <stdint.h>
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);
int uart_haschar(void);
#endif

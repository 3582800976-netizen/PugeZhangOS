// include/uart.h — 串口驱动接口

#ifndef __UART_H
#define __UART_H

#include "types.h"

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);

#endif /* __UART_H */

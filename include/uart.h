// include/uart.h — 串口驱动接口

#ifndef __UART_H
#define __UART_H

#include "types.h"

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);
void uart_enable_rx_irq(void);
void uart_intr(void);
uint64 uart_rx_bytes(void);
uint64 uart_rx_dropped(void);

#endif /* __UART_H */

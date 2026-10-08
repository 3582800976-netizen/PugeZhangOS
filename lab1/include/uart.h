/* Lab 1：直接轮询串口状态，不使用中断。 */
#ifndef PUGE_UART_H
#define PUGE_UART_H
void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);
#endif

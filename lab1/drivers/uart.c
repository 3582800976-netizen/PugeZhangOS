/* UART 是 QEMU 提供的串口设备；这里通过物理地址读写其寄存器。 */
#include "types.h"
#include "platform.h"
#include "uart.h"

#define DATA 0
#define IER 1
#define FCR 2
#define LCR 3
#define LSR 5
#define TX_READY 0x20
#define RX_READY 0x01

/* volatile 防止编译器把真实的设备访问当成普通变量访问省略。 */
static volatile uint8 *const uart = (volatile uint8 *)UART0_BASE;

void uart_init(void)
{
    uart[IER] = 0;              /* Lab 1 不开启串口中断 */
    uart[LCR] = 3;              /* 8 位数据，无校验，1 位停止位 */
    uart[FCR] = 7;              /* 开启 FIFO，并清理旧输入输出 */
}

void uart_putc(char c)
{
    while (!(uart[LSR] & TX_READY)) asm volatile("nop");
    uart[DATA] = (uint8)c;
}

void uart_puts(const char *s)
{
    for (uint64 i = 0; s[i] != '\0'; i++) uart_putc(s[i]);
}

int uart_getc(void)
{
    return (uart[LSR] & RX_READY) ? uart[DATA] : -1;
}

/* Lab 2 UART: read and write the QEMU virt NS16550 by polling status bits.
 * There is no interrupt handler or receive ring in this stage.
 */
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

static volatile uint8 *const uart = (volatile uint8 *)UART0_BASE;

void uart_init(void)
{
    uart[IER] = 0;  /* Keep hardware receive interrupts disabled. */
    uart[LCR] = 3;  /* 8 data bits, no parity, one stop bit; DLAB cleared. */
    uart[FCR] = 7;  /* Enable FIFO and clear old RX/TX data. */
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

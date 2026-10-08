// QEMU virt NS16550: polled TX, and an interrupt-fed receive ring in Lab 3.
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
#define RX_CAPACITY 256U

static volatile uint8 *const uart = (volatile uint8 *)UART0_BASE;
static uint8 received[RX_CAPACITY];
static uint32 read_position, write_position;
static uint64 rx_bytes, rx_dropped;

void uart_init(void)
{
    uart[IER] = 0;
    uart[LCR] = 3;  // 8 data bits, no parity, one stop bit; DLAB cleared.
    uart[FCR] = 7;  // Enable FIFO and clear old RX/TX data.
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

void uart_enable_rx_irq(void)
{
    uart[IER] = 1;  // Received-data interrupt; output remains synchronous.
}

void uart_intr(void)
{
    // PLIC serializes this one IRQ source. Main is the only ring consumer.
    while (uart[LSR] & RX_READY) {
        uint8 c = uart[DATA];
        __atomic_fetch_add(&rx_bytes, 1, __ATOMIC_RELAXED);
        uint32 write = __atomic_load_n(&write_position, __ATOMIC_RELAXED);
        uint32 read = __atomic_load_n(&read_position, __ATOMIC_ACQUIRE);
        if ((uint32)(write - read) == RX_CAPACITY) {
            __atomic_fetch_add(&rx_dropped, 1, __ATOMIC_RELAXED);
            continue;
        }
        received[write & (RX_CAPACITY - 1)] = c;
        __atomic_store_n(&write_position, write + 1, __ATOMIC_RELEASE);
    }
}

int uart_getc(void)
{
    uint32 read = __atomic_load_n(&read_position, __ATOMIC_RELAXED);
    uint32 write = __atomic_load_n(&write_position, __ATOMIC_ACQUIRE);
    if (read == write) return -1;
    int c = received[read & (RX_CAPACITY - 1)];
    __atomic_store_n(&read_position, read + 1, __ATOMIC_RELEASE);
    return c;
}

uint64 uart_rx_bytes(void) { return __atomic_load_n(&rx_bytes, __ATOMIC_RELAXED); }
uint64 uart_rx_dropped(void) { return __atomic_load_n(&rx_dropped, __ATOMIC_RELAXED); }

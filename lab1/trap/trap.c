/* 最小异常诊断：出错时报告位置并停机，避免默默卡住。 */
#include "riscv.h"
#include "trap.h"
#include "console.h"
#include "uart.h"

extern void kernel_vector(void);

void trap_init_hart(void)
{
    intr_off();
    w_sie(0);
    w_stvec((uint64)kernel_vector);
}

static void print_hex(uint64 value)
{
    for (int shift = 60; shift >= 0; shift -= 4)
        uart_putc("0123456789abcdef"[(value >> shift) & 15]);
}

void trap_handle(uint64 cause, uint64 pc, uint64 value)
{
    /* 异常可能出现在持有打印锁时，所以直接写串口，避免再次抢锁。 */
    uart_puts("trap: scause="); print_hex(cause);
    uart_puts(" sepc="); print_hex(pc);
    uart_puts(" stval="); print_hex(value);
    uart_puts("\n");
    panic("unexpected supervisor trap");
}

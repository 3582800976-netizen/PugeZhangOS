/* QEMU virt PLIC: hart h has M-context 2h, S-context 2h+1. */
#include "platform.h"
#include "riscv.h"
#include "plic.h"
static volatile uint32 *reg(uint64 offset)
{
    return (volatile uint32 *)(PLIC_BASE + offset);
}
static uint64 context(void) { return 2 * r_tp() + 1; }
void plic_init(void)
{
    *reg(UART_IRQ * 4) = 1;
    fence_io();
}
void plic_init_hart(void)
{
    uint64 ctx = context();
    /* Source 10 is in enable word 0. All other IRQs remain masked. */
    *reg(0x2000 + ctx * 0x80) = 1U << UART_IRQ;
    *reg(0x200000 + ctx * 0x1000) = 0; /* priority must be > threshold */
    fence_io();
}
uint32 plic_claim(void)
{
    uint32 irq = *reg(0x200004 + context() * 0x1000);
    fence_io();
    return irq;
}
void plic_complete(uint32 irq)
{
    fence_io();
    *reg(0x200004 + context() * 0x1000) = irq;
    fence_io();
}

/* Lab 1 用到的特权寄存器；CSR 是 CPU 内部的控制寄存器。 */
#ifndef PUGE_RISCV_H
#define PUGE_RISCV_H
#include "types.h"
#define MSTATUS_MPP_MASK (3UL << 11)
#define MSTATUS_MPP_S    (1UL << 11)
#define MSTATUS_MIE      (1UL << 3)

#define CSR_READ(name) \
static inline uint64 r_##name(void) { \
    uint64 value; \
    asm volatile("csrr %0, " #name : "=r"(value)); \
    return value; \
}
#define CSR_WRITE(name) \
static inline void w_##name(uint64 value) { \
    asm volatile("csrw " #name ", %0" : : "r"(value) : "memory"); \
}
CSR_READ(mstatus)
CSR_READ(satp)
CSR_READ(time)
CSR_WRITE(mstatus)
CSR_WRITE(mepc)
CSR_WRITE(mie)
CSR_WRITE(medeleg)
CSR_WRITE(mideleg)
CSR_WRITE(pmpaddr0)
CSR_WRITE(pmpcfg0)
CSR_WRITE(mcounteren)
CSR_WRITE(sie)
CSR_WRITE(stvec)
CSR_WRITE(satp)
#undef CSR_READ
#undef CSR_WRITE

static inline uint64 r_tp(void)
{
    uint64 value;
    asm volatile("mv %0, tp" : "=r"(value));
    return value;
}
static inline void w_tp(uint64 value)
{
    asm volatile("mv tp, %0" : : "r"(value) : "memory");
}
static inline void intr_off(void)
{
    asm volatile("csrci sstatus, 2" : : : "memory");
}
static inline void cpu_wait(void)
{
    asm volatile("wfi" : : : "memory");
}
static inline void fence_io(void)
{
    asm volatile("fence iorw, iorw" : : : "memory");
}
#endif

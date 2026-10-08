/* 只封装本实验使用的寄存器；明确标记编译器可见的副作用。 */
#ifndef PUGE_RISCV_H
#define PUGE_RISCV_H
#include "types.h"
#define SSTATUS_SIE  (1UL << 1)
#define SSTATUS_SPIE (1UL << 5)
#define SSTATUS_SPP  (1UL << 8)
#define SATP_SV39    (8UL << 60)
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
CSR_READ(sstatus)
CSR_READ(sie)
CSR_READ(satp)
CSR_READ(scause)
CSR_READ(sepc)
CSR_READ(stval)
CSR_READ(time)
CSR_WRITE(sstatus)
CSR_WRITE(sie)
CSR_WRITE(stvec)
CSR_WRITE(sscratch)
CSR_WRITE(sepc)
CSR_WRITE(satp)
#undef CSR_READ
#undef CSR_WRITE
static inline uint64 r_tp(void) {
    uint64 value;
    asm volatile("mv %0, tp" : "=r"(value));
    return value;
}
static inline void w_tp(uint64 value) {
    asm volatile("mv tp, %0" : : "r"(value) : "memory");
}
static inline void intr_off(void) {
    asm volatile("csrci sstatus, 2" : : : "memory");
}
static inline void intr_on(void) {
    asm volatile("csrsi sstatus, 2" : : : "memory");
}
static inline int intr_get(void) { return (r_sstatus() & SSTATUS_SIE) != 0; }
static inline void sfence_vma(void) {
    asm volatile("sfence.vma zero, zero" : : : "memory");
}
static inline void fence_io(void) {
    asm volatile("fence iorw, iorw" : : : "memory");
}
static inline void cpu_wait(void) { asm volatile("wfi" : : : "memory"); }
#endif

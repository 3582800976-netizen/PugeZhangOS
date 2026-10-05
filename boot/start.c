#include "platform.h"
#include "riscv.h"
#include "boottrace.h"
void kernel_main(uint64 hartid, uint64 dtb, int primary);
/* entry.S has already cleared BSS and prepared this hart's own stack. */
void boot_start(uint64 hartid, uint64 dtb, int primary)
{
    w_tp(hartid);
    boot_trace_mark(hartid, BOOT_ENTRY | BOOT_STACK);
#if LAB == 1
    /* -bios none: supply the M-mode setup normally done by OpenSBI. */
    w_mie(0);
    w_mcounteren(7);                  /* permit S-mode time/cycle/instret */
    w_pmpaddr0(~0UL);
    w_pmpcfg0(0x0f);                  /* TOR: all memory R/W/X */
    w_medeleg(0xffffUL & ~(1UL << 9)); /* leave S-mode ecall with M-mode */
    w_mideleg((1UL << 1) | (1UL << 5) | (1UL << 9));
    w_satp(0);
    w_sie(0);
    w_mstatus((r_mstatus() & ~(MSTATUS_MPP_MASK | MSTATUS_MIE)) |
              MSTATUS_MPP_S);
    w_mepc((uint64)kernel_main);
    register uint64 arg0 asm("a0") = hartid;
    register uint64 arg1 asm("a1") = dtb;
    register uint64 arg2 asm("a2") = (uint64)primary;
    asm volatile("mret" : : "r"(arg0), "r"(arg1), "r"(arg2) : "memory");
    __builtin_unreachable();
#else
    /* Firmware already configured PMP, delegation and mret; we are in S-mode. */
    intr_off();
    w_sie(0);
    w_satp(0);
    sfence_vma();
    kernel_main(hartid, dtb, primary);
#endif
    for (;;) cpu_wait();
}

#include "platform.h"
#include "riscv.h"
#include "boottrace.h"
void kernel_main(uint64 hartid, uint64 dtb, int primary);
/* entry.S has already cleared BSS and prepared this hart's own stack. */
void boot_start(uint64 hartid, uint64 dtb, int primary)
{
    w_tp(hartid);
    boot_trace_mark(hartid, BOOT_ENTRY | BOOT_STACK);
    /* Firmware already configured PMP, delegation and mret; we are in S-mode. */
    intr_off();
    w_sie(0);
    w_satp(0);
    sfence_vma();
    kernel_main(hartid, dtb, primary);
    for (;;) cpu_wait();
}

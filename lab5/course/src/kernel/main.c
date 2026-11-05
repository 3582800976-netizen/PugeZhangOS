#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"
#include "trap/mod.h"
#include "proc/method.h"

extern void _entry_secondary(void);
static int initialized;

void main(void)
{
    intr_off();
    uint64 hart = r_tp();
    if (hart == boot_hartid) {
        print_init();
        pmem_init();
        kvm_init();
        trap_kernel_init();
        mmap_init();
        __atomic_store_n(&initialized, 1, __ATOMIC_RELEASE);
        for (int id = FIRST_HART; id < NCPU; id++) {
            if ((uint64)id == hart) continue;
            sbi_ret_t result = sbi_hart_start(id, (uint64)_entry_secondary, 0);
            if (result.error != SBI_SUCCESS) panic("Lab5 HSM start failed");
        }
    } else {
        while (!__atomic_load_n(&initialized, __ATOMIC_ACQUIRE)) ;
    }
    kvm_inithart();
    trap_kernel_inithart();
    timer_init();
    if (hart == boot_hartid) uart_enable_rx_interrupt();
    intr_on();
    printf("cpu %d is ready with interrupts!\n", (int)hart);
    if (hart == boot_hartid) {
        proc_make_first();
        intr_on();
        printf("[lab5] user stopped; kernel remains alive\n");
    }
    for (;;) asm volatile("wfi");
}

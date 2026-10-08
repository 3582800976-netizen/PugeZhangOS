/* Startup orchestration only: each subsystem owns its state and behavior. */
#include "platform.h"
#include "riscv.h"
#include "console.h"
#include "boottrace.h"
#include "bootstate.h"
#include "dtb.h"
#include "sbi.h"
#include "page.h"
#include "vm.h"
#include "trap.h"
#include "selftest.h"
#include "monitor.h"

void kernel_main(uint64 id, uint64 dtb, int primary)
{
    boot_trace_mark(id, BOOT_SUPERVISOR);
    if (primary) {
        boot_set_leader(id);
        console_init();
        console_printf("\nPugeZhangOS: Lab 2, %d harts, boot hart %lu\n", CPUS, id);
        uint64 length = boot_dtb_size(dtb);
        if (!length) panic("invalid firmware DTB");
        console_printf("[firmware] OpenSBI DTB=0x%lx bytes=%lu preserved\n", dtb, length);
        page_init(dtb, length);
        if (vm_init()) panic("kernel page table creation failed");
        selftest_prepare();
        int started = sbi_boot_secondaries(id, dtb, CPUS);
        console_printf("[firmware] HSM started=%d\n", started);
        boot_trace_mark(id, BOOT_GLOBAL);
        boot_publish_global();
    } else boot_wait_global();

    vm_enable();
    boot_trace_mark(id, BOOT_PAGING);
    trap_init_hart();
    boot_trace_mark(id, BOOT_TRAP);
    selftest_hart(id, primary);
    boot_hart_ready(id);

    if (primary) {
        boot_wait_all();
        diagnostics_boot();
        int failures = selftest_parallel_result();
        diagnostics_memory();
        failures += selftest_run();
        if (failures) panic("boot self-tests failed");
        console_printf("[ready] lab=2 cpus=%d leader=%lu\n", CPUS, boot_leader());
        monitor_run();
    }
    for (;;) cpu_wait();
}

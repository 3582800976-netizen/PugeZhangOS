/* Lab 1 的启动顺序；具体机制、展示、自检和交互各有自己的模块。 */
#include "platform.h"
#include "riscv.h"
#include "console.h"
#include "boottrace.h"
#include "bootstate.h"
#include "trap.h"
#include "monitor.h"
#include "selftest.h"

void kernel_main(uint64 id, int primary)
{
    boot_trace_mark(id, BOOT_SUPERVISOR);
    if (primary) {
        boot_set_leader(id);
        console_init();
        console_printf("\nPugeZhangOS: Lab 1, %d harts, boot hart %lu\n", CPUS, id);
        boot_trace_mark(id, BOOT_GLOBAL);
        boot_publish_global();
    } else {
        boot_wait_global();
    }

    trap_init_hart();
    boot_trace_mark(id, BOOT_TRAP);
    selftest_hart(id, primary);
    boot_hart_ready(id);

    if (primary) {
        boot_wait_all();
        diagnostics_boot();
        if (selftest_run()) panic("boot self-tests failed");
        console_printf("[ready] lab=1 cpus=%d leader=%lu\n", CPUS, boot_leader());
        monitor_run();
    }
    for (;;) cpu_wait();
}

/* 这里只展示启动模块提供的快照，不直接操作多核共享状态。 */
#include "platform.h"
#include "console.h"
#include "bootstate.h"
#include "monitor.h"

void diagnostics_boot(void)
{
    for (unsigned id = 0; id < CPUS; id++) {
        struct boot_snapshot snapshot;
        boot_snapshot(id, &snapshot);
        console_printf("[boot] hart=%u stages=0x%x ready=%u stack=0x%lx..0x%lx satp=0x%lx\n",
                       id, snapshot.stages, snapshot.ready, snapshot.stack_low,
                       snapshot.stack_high, snapshot.satp);
    }
}

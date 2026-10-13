#ifndef PUGE_BOOTSTATE_H
#define PUGE_BOOTSTATE_H
#include "types.h"
struct boot_snapshot {
    uint32 stages, ready;
    uint64 satp, stack_low, stack_high;
};
void boot_set_leader(uint64 id);
uint64 boot_leader(void);
void boot_publish_global(void);
void boot_wait_global(void);
void boot_hart_ready(uint64 id);
void boot_wait_all(void);
/* id must name an active hart; unpublished stages/satp are reported as zero. */
void boot_snapshot(uint64 id, struct boot_snapshot *out);
#endif

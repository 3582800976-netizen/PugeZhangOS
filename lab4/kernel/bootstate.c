/* Shared boot facts and acquire/release publication between harts.
 * Every hart owns its record until boot_hart_ready publishes it.
 */
#include "bootstate.h"
#include "boottrace.h"
#include "platform.h"
#include "riscv.h"
#include "console.h"

extern char _boot_stack_top[];
struct boot_record { uint32 stages, ready; uint64 satp; };
static struct boot_record records[MAX_HARTS];
static uint32 global_ready, ready_count;
static uint64 leader;

void boot_set_leader(uint64 id) { leader = id; }
uint64 boot_leader(void) { return leader; }

void boot_trace_mark(uint64 id, uint32 event)
{
    records[id].stages |= event;
}

void boot_publish_global(void)
{
    __atomic_store_n(&global_ready, 1, __ATOMIC_RELEASE);
}

void boot_wait_global(void)
{
    while (!__atomic_load_n(&global_ready, __ATOMIC_ACQUIRE))
        asm volatile("nop");
}

void boot_hart_ready(uint64 id)
{
    records[id].satp = r_satp();
    boot_trace_mark(id, BOOT_READY);
    __atomic_store_n(&records[id].ready, 1, __ATOMIC_RELEASE);
    __atomic_fetch_add(&ready_count, 1, __ATOMIC_ACQ_REL);
}

void boot_wait_all(void)
{
    uint64 deadline = r_time() + 5UL * TIMEBASE_HZ;
    while (__atomic_load_n(&ready_count, __ATOMIC_ACQUIRE) != CPUS) {
        if (r_time() > deadline)
            panic("hart rendezvous timed out; CPUS must match QEMU -smp");
        asm volatile("nop");
    }
}

void boot_snapshot(uint64 id, struct boot_snapshot *out)
{
    if (id >= CPUS || !out) panic("invalid boot snapshot");
    /* Acquire ready before reading the fields that this hart published. */
    out->ready = __atomic_load_n(&records[id].ready, __ATOMIC_ACQUIRE);
    out->stages = out->ready ? records[id].stages : 0;
    out->satp = out->ready ? records[id].satp : 0;
    out->stack_high = (uint64)_boot_stack_top - id * BOOT_STACK_SIZE;
    out->stack_low = out->stack_high - BOOT_STACK_SIZE;
}

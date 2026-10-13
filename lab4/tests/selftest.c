/* Test orchestration is separate from the normal startup and monitor paths.
 * Module-level self-tests stay beside their private implementation details.
 */
#include "selftest.h"
#include "platform.h"
#include "riscv.h"
#include "console.h"
#include "page.h"
#include "vm.h"
#include "trap.h"

static void *held_kernel[MAX_HARTS], *held_user[MAX_HARTS];
static uint32 held_count, allow_release;
static int hart_page_failures[MAX_HARTS];
static uint64 baseline_free[2];

static void wait_count(uint32 *count, uint32 target)
{
    uint64 deadline = r_time() + 5UL * TIMEBASE_HZ;
    while (__atomic_load_n(count, __ATOMIC_ACQUIRE) != target) {
        if (r_time() > deadline) panic("hart rendezvous timed out; CPUS must match QEMU -smp");
        asm volatile("nop");
    }
}

static void parallel_pages(uint64 id, int primary)
{
    int failed = 0;
    held_kernel[id] = page_alloc(PAGE_KERNEL);
    held_user[id] = page_alloc(PAGE_USER);
    if (!held_kernel[id] || !held_user[id]) failed++;
    __atomic_fetch_add(&held_count, 1, __ATOMIC_ACQ_REL);
    if (primary) {
        wait_count(&held_count, CPUS);
        for (unsigned i = 0; i < CPUS; i++)
            for (unsigned j = 0; j < i; j++)
                if (held_kernel[i] == held_kernel[j] || held_user[i] == held_user[j]) failed++;
        __atomic_store_n(&allow_release, 1, __ATOMIC_RELEASE);
    } else {
        while (!__atomic_load_n(&allow_release, __ATOMIC_ACQUIRE)) asm volatile("nop");
    }
    if (held_kernel[id]) failed += page_free(PAGE_KERNEL, held_kernel[id]) != PAGE_OK;
    if (held_user[id]) failed += page_free(PAGE_USER, held_user[id]) != PAGE_OK;
    // All harts now repeatedly contend on both pool locks.
    for (unsigned i = 0; i < 64; i++) {
        enum page_pool pool = (i & 1) ? PAGE_USER : PAGE_KERNEL;
        uint64 *p = page_alloc(pool);
        if (!p) { failed++; continue; }
        p[0] = 0xfeed0000UL + id;
        for (unsigned delay = 0; delay < 30; delay++) asm volatile("nop");
        if (p[0] != 0xfeed0000UL + id) failed++;
        failed += page_free(pool, p) != PAGE_OK;
    }
    hart_page_failures[id] = failed;
}

int selftest_run(void)
{
    int failures = console_selftest();
    int page_fail = page_selftest();
    console_printf("[test] page %s\n", page_fail ? "FAIL" : "PASS");
    failures += page_fail;
    int vm_fail = vm_selftest();
    console_printf("[test] vm %s\n", vm_fail ? "FAIL" : "PASS");
    failures += vm_fail;
    int trap_fail = trap_selftest();
    console_printf("[test] trap %s\n", trap_fail ? "FAIL" : "PASS");
    failures += trap_fail;
    console_printf("[test] summary failures=%d\n", failures);
    return failures;
}


void selftest_prepare(void)
{
    for (int pool = 0; pool < 2; pool++) {
        struct page_stats s;
        page_get_stats(pool, &s);
        baseline_free[pool] = s.free;
    }
}

void selftest_hart(uint64 id, int primary)
{
    for (unsigned seq = 0; seq < 4; seq++)
        console_printf("[uart-test] hart=%lu seq=%u\n", id, seq);
    parallel_pages(id, primary);
}

int selftest_parallel_result(void)
{
    int failed = 0;
    for (unsigned i = 0; i < CPUS; i++) failed += hart_page_failures[i];
    for (int pool = 0; pool < 2; pool++) {
        struct page_stats s;
        page_get_stats(pool, &s);
        failed += s.free != baseline_free[pool];
    }
    console_printf("[test] page-concurrent %s\n", failed ? "FAIL" : "PASS");
    return failed;
}

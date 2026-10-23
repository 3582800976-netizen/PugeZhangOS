/* These checks use the production interval repository, page tables and page
 * allocator. Test objects live in BSS: the process kernel stack is one page. */
#include "user_heap.h"
#include "process.h"
#include "user_layout.h"
#include "page.h"
#include "memory.h"
#include "console.h"

extern char _user_code_start[], _user_code_end[];
extern char _user_data_start[], _user_data_end[];

static struct process fixture, holders[N_MMAP + 1];
static struct user_heap_snapshot snapshots[2];
static struct {
    struct user_heap_snapshot heap;
    struct vm_mapping marker;
    uint64 free[2];
    int have_marker;
} saved;

static int check(const char *name, int passed)
{
    console_printf("[htest] %s %s\n", name, passed ? "PASS" : "FAIL");
    return !passed;
}

static void free_counts(uint64 counts[2])
{
    for (int pool = PAGE_KERNEL; pool <= PAGE_USER; pool++) {
        struct page_stats stats;
        page_get_stats(pool, &stats);
        counts[pool] = stats.free;
    }
}

static int same_counts(const uint64 counts[2])
{
    uint64 now[2];
    free_counts(now);
    return now[0] == counts[0] && now[1] == counts[1];
}

static int bytes_are(const void *buffer, uint64 length, uint8 value)
{
    const uint8 *bytes = buffer;
    for (uint64 i = 0; i < length; i++)
        if (bytes[i] != value)
            return 0;
    return 1;
}

static int same_bytes(const void *left, const void *right, uint64 length)
{
    const uint8 *a = left, *b = right;
    for (uint64 i = 0; i < length; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static int snapshot(unsigned index)
{
    return user_heap_snapshot(&fixture, &snapshots[index]) == 0;
}

static int one_region(uint64 begin, uint64 npages)
{
    return snapshot(0) && snapshots[0].info.regions == 1 &&
           snapshots[0].regions[0].begin == begin &&
           snapshots[0].regions[0].npages == npages &&
           snapshots[0].info.free_pages == npages &&
           snapshots[0].info.allocated_pages == USER_HEAP_PAGES - npages &&
           snapshots[0].info.largest_free_pages == npages;
}

static int mark(uint64 va, uint8 value)
{
    struct vm_mapping mapping;
    if (user_vm_lookup(&fixture.vm, va, &mapping) != VM_OK)
        return 0;
    memset((void *)mapping.pa, value, PAGE_SIZE);
    return 1;
}

static int zero_rwu_nx(uint64 begin, uint64 npages)
{
    for (uint64 i = 0; i < npages; i++) {
        struct vm_mapping mapping;
        if (user_vm_lookup(&fixture.vm, begin + i * PAGE_SIZE, &mapping) != VM_OK ||
            (mapping.flags & (VM_R | VM_W | VM_X | VM_U)) != (VM_R | VM_W | VM_U) ||
            !bytes_are((const void *)mapping.pa, PAGE_SIZE, 0))
            return 0;
    }
    return 1;
}

static int save_state(void)
{
    free_counts(saved.free);
    saved.have_marker = user_vm_lookup(&fixture.vm, USER_HEAP_BASE,
                                       &saved.marker) == VM_OK;
    return user_heap_snapshot(&fixture, &saved.heap) == 0;
}

static int same_state(void)
{
    struct vm_mapping mapping;
    int marker = user_vm_lookup(&fixture.vm, USER_HEAP_BASE, &mapping) == VM_OK;
    if (marker != saved.have_marker ||
        (marker && (mapping.pa != saved.marker.pa || mapping.pte != saved.marker.pte ||
                    !bytes_are((const void *)mapping.pa, PAGE_SIZE, 0x6b))))
        return 0;
    return same_counts(saved.free) && snapshot(1) &&
           same_bytes(&snapshots[1], &saved.heap, sizeof saved.heap);
}

static int split_and_merge(void)
{
    int failures = 0;
    uint64 out = ~0UL;
    int good = one_region(USER_HEAP_BASE, USER_HEAP_PAGES) &&
               snapshots[0].info.base == USER_HEAP_BASE &&
               snapshots[0].info.limit == USER_HEAP_LIMIT &&
               snapshots[0].info.total_pages == USER_HEAP_PAGES;
    failures += check("initial-region", good);

    good = user_heap_alloc(&fixture, 0, 2, &out, -1) == 0 &&
           out == USER_HEAP_BASE &&
           one_region(USER_HEAP_BASE + 2 * PAGE_SIZE, USER_HEAP_PAGES - 2);
    failures += check("front-allocation", good);
    failures += check("zero-rwu-nx", zero_rwu_nx(USER_HEAP_BASE, 2));

    uint64 middle = USER_HEAP_BASE + 8 * PAGE_SIZE;
    good = user_heap_alloc(&fixture, middle, 2, &out, -1) == 0 && out == middle &&
           snapshot(0) && snapshots[0].info.regions == 2 &&
           snapshots[0].regions[0].begin == USER_HEAP_BASE + 2 * PAGE_SIZE &&
           snapshots[0].regions[0].npages == 6 &&
           snapshots[0].regions[1].begin == middle + 2 * PAGE_SIZE &&
           snapshots[0].regions[1].npages == USER_HEAP_PAGES - 10;
    failures += check("middle-split", good);

    good = user_heap_alloc(&fixture, USER_HEAP_LIMIT - 2 * PAGE_SIZE, 2,
                           &out, -1) == 0 && out == USER_HEAP_LIMIT - 2 * PAGE_SIZE &&
           snapshot(0) && snapshots[0].regions[1].npages == USER_HEAP_PAGES - 12;
    failures += check("end-allocation", good);

    good = user_heap_alloc(&fixture, USER_HEAP_BASE + 2 * PAGE_SIZE, 6,
                           &out, -1) == 0 && out == USER_HEAP_BASE + 2 * PAGE_SIZE &&
           one_region(USER_HEAP_BASE + 10 * PAGE_SIZE, USER_HEAP_PAGES - 12);
    failures += check("whole-region", good);

    good = user_heap_free(&fixture, USER_HEAP_BASE + 2 * PAGE_SIZE, 6) == 0 &&
           snapshot(0) && snapshots[0].info.regions == 2 &&
           snapshots[0].regions[0].begin == USER_HEAP_BASE + 2 * PAGE_SIZE &&
           snapshots[0].regions[0].npages == 6;
    failures += check("isolated-insert", good);

    good = user_heap_free(&fixture, USER_HEAP_BASE, 2) == 0 && snapshot(0) &&
           snapshots[0].info.regions == 2 &&
           snapshots[0].regions[0].begin == USER_HEAP_BASE &&
           snapshots[0].regions[0].npages == 8;
    failures += check("right-merge", good);

    good = user_heap_free(&fixture, middle, 2) == 0 &&
           one_region(USER_HEAP_BASE, USER_HEAP_PAGES - 2);
    failures += check("double-merge", good);
    good = user_heap_free(&fixture, USER_HEAP_LIMIT - 2 * PAGE_SIZE, 2) == 0 &&
           one_region(USER_HEAP_BASE, USER_HEAP_PAGES);
    failures += check("left-merge", good);

    good = user_heap_alloc(&fixture, USER_HEAP_BASE + 2 * PAGE_SIZE, 1,
                           &out, -1) == 0 &&
           user_heap_alloc(&fixture, 0, 3, &out, -1) == 0 &&
           out == USER_HEAP_BASE + 3 * PAGE_SIZE &&
           user_heap_free(&fixture, out, 3) == 0 &&
           user_heap_free(&fixture, USER_HEAP_BASE + 2 * PAGE_SIZE, 1) == 0 &&
           one_region(USER_HEAP_BASE, USER_HEAP_PAGES);
    failures += check("first-fit-skips-small", good);
    return failures;
}

static int invalid_and_rollback(void)
{
    int failures = 0;
    uint64 out = 0;
    int setup = user_heap_alloc(&fixture, 0, 1, &out, -1) == 0 &&
                out == USER_HEAP_BASE && mark(USER_HEAP_BASE, 0x6b);
    failures += check("rollback-existing-data", setup);
    if (!setup)
        return failures;

    static const struct { uint64 hint, npages; int budget; } invalid[] = {
        {0, 0, -1}, {0, USER_HEAP_PAGES + 1, -1},
        {USER_HEAP_BASE + 1, 1, -1}, {USER_HEAP_BASE - PAGE_SIZE, 1, -1},
        {USER_HEAP_LIMIT, 1, -1}, {USER_HEAP_LIMIT - PAGE_SIZE, 2, -1},
        {~0UL - PAGE_SIZE + 1, 2, -1}, {USER_HEAP_BASE, ~0UL, -1},
        {0, 1, -2}
    };
    int good = save_state();
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        out = 0x12345;
        int result = user_heap_alloc(&fixture, invalid[i].hint, invalid[i].npages,
                                     &out, invalid[i].budget);
        good &= result == SYS_EINVAL && out == 0x12345 && same_state();
    }
    good &= user_heap_alloc(&fixture, 0, 1, 0, -1) == SYS_EINVAL && same_state();
    good &= user_heap_alloc(0, 0, 1, &out, -1) == SYS_EINVAL && same_state();
    good &= user_heap_create(&fixture) == SYS_EINVAL && same_state();
    good &= user_heap_create(0) == SYS_EINVAL && same_state();
    good &= user_heap_alloc(&holders[0], 0, 1, &out, -1) == SYS_EINVAL && same_state();
    good &= user_heap_free(&holders[0], USER_HEAP_BASE, 1) == SYS_EINVAL && same_state();
    good &= user_heap_snapshot(&holders[0], &snapshots[0]) == SYS_EINVAL && same_state();
    good &= user_heap_snapshot(&fixture, 0) == SYS_EINVAL && same_state();
    static const struct { uint64 begin, npages; } bad_frees[] = {
        {USER_HEAP_BASE, 0}, {USER_HEAP_BASE + 1, 1},
        {USER_HEAP_BASE - PAGE_SIZE, 1}, {USER_HEAP_LIMIT, 1},
        {USER_HEAP_LIMIT - PAGE_SIZE, 2}, {USER_HEAP_BASE, ~0UL},
        {~0UL - PAGE_SIZE + 1, 2}
    };
    for (unsigned i = 0; i < sizeof bad_frees / sizeof bad_frees[0]; i++)
        good &= user_heap_free(&fixture, bad_frees[i].begin,
                               bad_frees[i].npages) == SYS_EINVAL && same_state();
    failures += check("invalid-arguments-atomic", good);

    out = 0x12345;
    good = user_heap_alloc(&fixture, USER_HEAP_BASE, 1, &out, -1) == SYS_ENOMEM &&
           out == 0x12345 && same_state();
    failures += check("occupied-hint-atomic", good);

    good = user_heap_alloc(&fixture, 0, 1, &out, -1) == 0 &&
           user_heap_free(&fixture, out, 1) == 0 && save_state();
    good &= user_heap_free(&fixture, out, 1) == SYS_EINVAL && same_state();
    failures += check("duplicate-free-atomic", good);
    good = user_heap_free(&fixture, USER_HEAP_BASE, 2) == SYS_EINVAL && same_state();
    failures += check("partial-overlap-free-atomic", good);

    /* A pre-existing VM leaf in an otherwise free interval forces the real
     * VM map validation to reject after the heap has reserved a split node. */
    uint64 collision = USER_HEAP_BASE + 10 * PAGE_SIZE;
    struct vm_mapping before, after;
    good = user_vm_heap_alloc(&fixture.vm, collision, 1, -1) == VM_OK &&
           mark(collision, 0x97) &&
           user_vm_lookup(&fixture.vm, collision, &before) == VM_OK && save_state();
    out = 0x12345;
    good &= user_heap_alloc(&fixture, collision, 1, &out, -1) == SYS_EINVAL &&
            out == 0x12345 && same_state() &&
            user_vm_lookup(&fixture.vm, collision, &after) == VM_OK &&
            after.pa == before.pa && after.pte == before.pte &&
            bytes_are((const void *)after.pa, PAGE_SIZE, 0x97);
    failures += check("failed-map-atomic", good);
    failures += check("collision-cleanup", user_vm_heap_free(&fixture.vm, collision, 1) == VM_OK);

    /* Break a later leaf deliberately, then verify free has no early effect.
     * Restore it through the production VM API before normal heap cleanup. */
    uint64 broken = USER_HEAP_BASE + 3 * PAGE_SIZE;
    struct vm_mapping first, last;
    good = user_heap_alloc(&fixture, broken, 3, &out, -1) == 0 &&
           user_vm_lookup(&fixture.vm, broken, &first) == VM_OK &&
           user_vm_lookup(&fixture.vm, broken + 2 * PAGE_SIZE, &last) == VM_OK &&
           user_vm_heap_free(&fixture.vm, broken + PAGE_SIZE, 1) == VM_OK && save_state();
    good &= user_heap_free(&fixture, broken, 3) == SYS_EINVAL && same_state() &&
            user_vm_lookup(&fixture.vm, broken, &after) == VM_OK && after.pa == first.pa &&
            user_vm_lookup(&fixture.vm, broken + 2 * PAGE_SIZE, &after) == VM_OK &&
            after.pa == last.pa;
    failures += check("bad-later-leaf-free-atomic", good);
    good = user_vm_heap_alloc(&fixture.vm, broken + PAGE_SIZE, 1, -1) == VM_OK &&
           user_heap_free(&fixture, broken, 3) == 0;
    failures += check("bad-leaf-cleanup", good);

    /* The initial low branch already exists. 0x1ff000 spans into a fresh
     * 2 MiB leaf table: three frames plus one page-table page are required. */
    for (int budget = 0; budget <= 4; budget++) {
        good = save_state();
        out = 0x12345;
        int result = user_heap_alloc(&fixture, 0x1ff000, 3, &out, budget);
        if (budget < 4) {
            good &= result == SYS_ENOMEM && out == 0x12345 && same_state() &&
                    user_vm_lookup(&fixture.vm, 0x1ff000, 0) == VM_UNMAPPED &&
                    user_vm_lookup(&fixture.vm, 0x200000, 0) == VM_UNMAPPED &&
                    user_vm_lookup(&fixture.vm, 0x201000, 0) == VM_UNMAPPED;
        } else {
            good &= result == 0 && out == 0x1ff000 && zero_rwu_nx(out, 3);
            good &= user_heap_free(&fixture, 0x1ff000, 3) == 0 && same_state();
        }
        console_printf("[htest] allocation-budget=%d %s\n", budget, good ? "PASS" : "FAIL");
        failures += !good;
    }
    return failures;
}

static int node_exhaustion(void)
{
    int failures = 0;
    uint64 out = 0;
    uint64 island = USER_HEAP_BASE + 8 * PAGE_SIZE;
    int good = user_heap_alloc(&fixture, island, 3, &out, -1) == 0;
    failures += check("node-exhaustion-setup", good);
    if (!good)
        return failures;
    memset(holders, 0, sizeof holders);
    unsigned held = 0;
    while (held < N_MMAP && user_heap_create(&holders[held]) == 0)
        ++held;
    good = user_heap_nodes_free() == 0 && held == N_MMAP - 2 && save_state();
    good &= user_heap_create(&holders[N_MMAP]) == SYS_ENOMEM &&
            !holders[N_MMAP].mmap && !holders[N_MMAP].heap_initialized && same_state();
    failures += check("node-exhaustion-create", good);

    out = 0x12345;
    good = user_heap_alloc(&fixture, USER_HEAP_BASE + 20 * PAGE_SIZE, 1,
                           &out, -1) == SYS_ENOMEM && out == 0x12345 && same_state();
    failures += check("node-exhaustion-split-atomic", good);
    good = user_heap_free(&fixture, island + PAGE_SIZE, 1) == SYS_ENOMEM && same_state();
    failures += check("node-exhaustion-insert-atomic", good);

    good = user_heap_alloc(&fixture, 0, 1, &out, -1) == 0 &&
           out == USER_HEAP_BASE + PAGE_SIZE && user_heap_nodes_free() == 0;
    failures += check("node-exhaustion-edge-allocation", good);
    good = user_heap_free(&fixture, USER_HEAP_BASE + PAGE_SIZE, 1) == 0 &&
           user_heap_nodes_free() == 0 && same_state();
    failures += check("node-exhaustion-adjacent-free", good);

    good = user_heap_alloc(&fixture, 0, 7, &out, -1) == 0 &&
           out == USER_HEAP_BASE + PAGE_SIZE && user_heap_nodes_free() == 1;
    failures += check("node-exhaustion-whole-allocation", good);
    good = user_heap_free(&fixture, island, 3) == 0 &&
           user_heap_free(&fixture, USER_HEAP_BASE + PAGE_SIZE, 7) == 0;
    for (unsigned i = 0; i < held; i++)
        user_heap_destroy(&holders[i]);
    good &= one_region(USER_HEAP_BASE + PAGE_SIZE, USER_HEAP_PAGES - 1) &&
            user_heap_nodes_free() == N_MMAP - 1;
    failures += check("node-exhaustion-recovery", good);
    return failures;
}

static int full_and_reuse(void)
{
    int failures = 0;
    uint64 out = 0;
    int good = user_heap_free(&fixture, USER_HEAP_BASE, 1) == 0 &&
               user_heap_alloc(&fixture, 0, USER_HEAP_PAGES, &out, -1) == 0 &&
               out == USER_HEAP_BASE && !fixture.mmap && fixture.heap_initialized &&
               snapshot(0) && snapshots[0].info.regions == 0 &&
               snapshots[0].info.free_pages == 0 &&
               snapshots[0].info.allocated_pages == USER_HEAP_PAGES &&
               snapshots[0].info.largest_free_pages == 0 &&
               user_heap_nodes_free() == N_MMAP;
    failures += check("full-allocation-empty-head", good);
    out = 0x12345;
    good = user_heap_alloc(&fixture, 0, 1, &out, -1) == SYS_ENOMEM &&
           out == 0x12345 && !fixture.mmap && fixture.heap_initialized &&
           user_heap_nodes_free() == N_MMAP;
    failures += check("full-allocation-no-space", good);
    good = mark(USER_HEAP_BASE, 0xa5) &&
           user_heap_free(&fixture, USER_HEAP_BASE, USER_HEAP_PAGES) == 0 &&
           one_region(USER_HEAP_BASE, USER_HEAP_PAGES) &&
           user_heap_nodes_free() == N_MMAP - 1;
    failures += check("full-allocation-release", good);
    good = user_heap_alloc(&fixture, 0, 2, &out, -1) == 0 &&
           out == USER_HEAP_BASE && zero_rwu_nx(out, 2) &&
           mark(out, 0xa5) && mark(out + PAGE_SIZE, 0x97) &&
           user_heap_free(&fixture, out, 2) == 0 &&
           user_heap_alloc(&fixture, 0, 2, &out, -1) == 0 &&
           out == USER_HEAP_BASE && zero_rwu_nx(out, 2) &&
           user_heap_free(&fixture, out, 2) == 0;
    failures += check("reuse-is-zero", good);

    uint64 baseline[2];
    free_counts(baseline);
    uint64 nodes = user_heap_nodes_free();
    good = 1;
    for (unsigned round = 0; round < 16; round++) {
        out = 0;
        good &= user_heap_alloc(&fixture, 0x1ff000, 3, &out, -1) == 0 &&
                out == 0x1ff000 && zero_rwu_nx(out, 3) &&
                mark(0x200000, (uint8)(round + 1));
        good &= user_heap_free(&fixture, 0x1ff000, 3) == 0 &&
                same_counts(baseline) && user_heap_nodes_free() == nodes &&
                one_region(USER_HEAP_BASE, USER_HEAP_PAGES);
    }
    failures += check("cross-2mib-reclaim-loops", good);
    return failures;
}

int user_heap_selftest(void)
{
    int failures = 0;
    uint64 initial[2], borrowed[2];
    uint64 initial_nodes = user_heap_nodes_free();
    free_counts(initial);
    void *frame = page_alloc(PAGE_KERNEL), *stack = page_alloc(PAGE_KERNEL);
    memset(&fixture, 0, sizeof fixture);
    if (!frame || !stack) {
        failures += check("borrowed-page-allocation", 0);
        goto cleanup;
    }
    memset(frame, 0x5a, PAGE_SIZE);
    memset(stack, 0xa5, PAGE_SIZE);
    free_counts(borrowed);
    int result = user_vm_create(&fixture.vm, _user_code_start,
                                (uint64)_user_code_end - (uint64)_user_code_start,
                                _user_data_start,
                                (uint64)_user_data_end - (uint64)_user_data_start,
                                (uint64)frame, (uint64)stack, -1);
    if (result != VM_OK || user_heap_create(&fixture) != 0) {
        failures += check("production-create", 0);
        goto cleanup;
    }
    failures += check("production-create", 1);
    failures += split_and_merge();
    failures += invalid_and_rollback();
    failures += node_exhaustion();
    failures += full_and_reuse();

    /* Exit while pages are live. Heap destruction returns metadata only;
     * the following VM destruction owns all remaining dynamic frames. */
    uint64 out = 0;
    int good = user_heap_alloc(&fixture, 0x1ff000, 3, &out, -1) == 0 &&
               user_heap_alloc(&fixture, 0, 2, &out, -1) == 0;
    user_heap_destroy(&fixture);
    good &= !fixture.mmap && !fixture.heap_initialized &&
            user_heap_nodes_free() == initial_nodes &&
            user_vm_lookup(&fixture.vm, 0x200000, 0) == VM_OK;
    user_vm_destroy(&fixture.vm);
    good &= same_counts(borrowed);
    failures += check("destroy-live-heap", good);
    user_heap_destroy(&fixture);
    user_vm_destroy(&fixture.vm);
    failures += check("metadata-repeat-destroy", same_counts(borrowed) &&
                      user_heap_nodes_free() == initial_nodes);
    failures += check("borrowed-content-preserved",
                      bytes_are(frame, PAGE_SIZE, 0x5a) && bytes_are(stack, PAGE_SIZE, 0xa5));

cleanup:
    for (unsigned i = 0; i <= N_MMAP; i++)
        user_heap_destroy(&holders[i]);
    user_heap_destroy(&fixture);
    user_vm_destroy(&fixture.vm);
    if (frame)
        page_free(PAGE_KERNEL, frame);
    if (stack)
        page_free(PAGE_KERNEL, stack);
    failures += check("resource-balance", same_counts(initial) &&
                      user_heap_nodes_free() == initial_nodes);
    console_printf("[htest] summary failures=%d\n", failures);
    return failures;
}

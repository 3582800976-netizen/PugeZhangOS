/* Exercise the production memory path inside the QEMU kernel. These pages
 * belong only to this test; no process is running on the temporary root. */
#include "user_vm.h"
#include "user_layout.h"
#include "page.h"
#include "memory.h"
#include "console.h"

extern char _user_code_start[], _user_code_end[];
extern char _user_data_start[], _user_data_end[];
extern char trampoline_start[];

static int check(const char *name, int passed)
{
    console_printf("[utest] memory-%s %s\n", name, passed ? "PASS" : "FAIL");
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

static int same_counts(const uint64 expected[2])
{
    uint64 actual[2];
    free_counts(actual);
    return actual[0] == expected[0] && actual[1] == expected[1];
}

static int has_mapping(const struct user_vm *vm, uint64 va, uint64 pa, uint64 flags)
{
    struct vm_mapping m;
    return user_vm_lookup(vm, va, &m) == VM_OK && m.pa == pa && m.level == 0 &&
           (m.flags & (VM_R | VM_W | VM_X | VM_U)) == flags;
}

static int bytes_are(const uint8 *bytes, uint64 len, uint8 value)
{
    for (uint64 i = 0; i < len; i++)
        if (bytes[i] != value)
            return 0;
    return 1;
}

static int rejected_without_copy(const struct user_vm *vm, uint64 address)
{
    uint8 destination[8];
    memset(destination, 0xa5, sizeof destination);
    int result = user_vm_copyin(vm, destination, address, sizeof destination);
    return result != VM_OK && bytes_are(destination, sizeof destination, 0xa5);
}

static int empty(const struct user_vm *vm)
{
    return !vm->root && !vm->code && !vm->data && !vm->stack;
}

/* Only used after lookup has confirmed a normal 4 KiB mapping. Tests may
 * edit their own inactive tree to exercise copy permission/PA checks. */
static uint64 *test_leaf(struct user_vm *vm, uint64 va)
{
    uint64 *table = (uint64 *)vm->root;
    for (int level = 2; level > 0; level--) {
        uint64 entry = table[(va >> (12 + 9 * level)) & 511];
        table = (uint64 *)((entry >> 10) << 12);
    }
    return &table[(va >> 12) & 511];
}

static int bytes_equal(const uint8 *a, const uint8 *b, uint64 len)
{
    for (uint64 i = 0; i < len; i++)
        if (a[i] != b[i])
            return 0;
    return 1;
}

static int heap_vm_tests(struct user_vm *vm, uint64 borrowed_frame)
{
    int failures = 0;
    uint64 baseline[2];
    free_counts(baseline);
    int result = user_vm_heap_alloc(vm, USER_HEAP_BASE, 2, -1);
    failures += check("heap-two-page-allocation", result == VM_OK);
    if (result != VM_OK)
        return failures;

    struct vm_mapping first, second;
    int mapped = user_vm_lookup(vm, USER_HEAP_BASE, &first) == VM_OK &&
                 user_vm_lookup(vm, USER_HEAP_BASE + PAGE_SIZE, &second) == VM_OK;
    failures += check("heap-zeroed-rwu-nx", mapped && first.pa != second.pa &&
                      has_mapping(vm, USER_HEAP_BASE, first.pa, VM_R | VM_W | VM_U) &&
                      has_mapping(vm, USER_HEAP_BASE + PAGE_SIZE, second.pa, VM_R | VM_W | VM_U) &&
                      bytes_are((const uint8 *)first.pa, PAGE_SIZE, 0) &&
                      bytes_are((const uint8 *)second.pa, PAGE_SIZE, 0));
    if (!mapped)
        return failures;

    const uint8 source[8] = {'1', '2', '3', '4', '5', '6', '7', '8'};
    uint8 copied[8];
    uint64 crossing = USER_HEAP_BASE + PAGE_SIZE - 4;
    int good = user_vm_copyout(vm, crossing, source, sizeof source) == VM_OK &&
               user_vm_copyin(vm, copied, crossing, sizeof copied) == VM_OK &&
               bytes_equal(copied, source, sizeof copied);
    failures += check("copyout-cross-page", good);

    uint64 *leaf = test_leaf(vm, USER_HEAP_BASE + PAGE_SIZE);
    uint64 saved = *leaf;
    memset((void *)(first.pa + PAGE_SIZE - 4), 0xa5, 4);
    memset((void *)second.pa, 0xa5, 4);
    *leaf &= ~(uint64)VM_W;
    result = user_vm_copyout(vm, crossing, source, sizeof source);
    good = result != VM_OK && bytes_are((uint8 *)(first.pa + PAGE_SIZE - 4), 4, 0xa5) &&
           bytes_are((uint8 *)second.pa, 4, 0xa5);
    *leaf = saved;
    failures += check("copyout-readonly-tail-no-partial-write", good);

    memset((void *)(second.pa + PAGE_SIZE - 4), 0xa5, 4);
    result = user_vm_copyout(vm, crossing + PAGE_SIZE, source, sizeof source);
    failures += check("copyout-unmapped-tail-no-partial-write", result != VM_OK &&
                      bytes_are((uint8 *)(second.pa + PAGE_SIZE - 4), 4, 0xa5));

    /* A U flag alone cannot turn a supervisor-pool physical page into a
     * legal copy target or source. Restore the leaf before other tests. */
    *leaf = ((borrowed_frame >> 12) << 10) | (saved & 0x3ffUL);
    memset(copied, 0xa5, sizeof copied);
    good = user_vm_copyin(vm, copied, USER_HEAP_BASE + PAGE_SIZE, sizeof copied) != VM_OK &&
           bytes_are(copied, sizeof copied, 0xa5) &&
           user_vm_copyout(vm, USER_HEAP_BASE + PAGE_SIZE, source, sizeof source) != VM_OK &&
           bytes_are((uint8 *)borrowed_frame, PAGE_SIZE, 0x5a);
    *leaf = saved;
    failures += check("user-copy-rejects-kernel-physical-page", good);

    memcpy(copied, (void *)vm->code, sizeof copied);
    failures += check("copyout-code-readonly", user_vm_copyout(vm, USER_CODE_BASE, source, sizeof source) != VM_OK &&
                      bytes_equal(copied, (uint8 *)vm->code, sizeof copied));
    failures += check("copyout-supervisor-pages", user_vm_copyout(vm, TRAPFRAME, source, sizeof source) != VM_OK &&
                      user_vm_copyout(vm, TRAMPOLINE, source, sizeof source) != VM_OK &&
                      user_vm_copyout(vm, KERNEL_STACK_BASE, source, sizeof source) != VM_OK);
    failures += check("copyout-wrap-rejected", user_vm_copyout(vm, ~0UL - 3, source, sizeof source) != VM_OK &&
                      user_vm_copyout(vm, (uint64)USER_VA_MAX - 4, source, sizeof source) != VM_OK);
    failures += check("copyout-zero-length-no-access", user_vm_copyout(0, ~0UL, 0, 0) == VM_OK);

    uint64 live[2];
    free_counts(live);
    result = user_vm_heap_free(vm, USER_HEAP_BASE, 3);
    failures += check("heap-free-whole-range-preflight", result == VM_UNMAPPED && same_counts(live) &&
                      has_mapping(vm, USER_HEAP_BASE, first.pa, VM_R | VM_W | VM_U) &&
                      has_mapping(vm, USER_HEAP_BASE + PAGE_SIZE, second.pa, VM_R | VM_W | VM_U));

    /* A later overlap must be found before any earlier hole is mapped. */
    uint64 extra = USER_HEAP_BASE + 4 * PAGE_SIZE;
    result = user_vm_heap_alloc(vm, extra, 1, -1);
    failures += check("heap-extra-live-page", result == VM_OK);
    free_counts(live);
    int overlap = user_vm_heap_alloc(vm, extra - PAGE_SIZE, 2, -1);
    failures += check("heap-alloc-whole-range-preflight", result == VM_OK && overlap == VM_ALREADY_MAPPED &&
                      same_counts(live) && user_vm_lookup(vm, extra - PAGE_SIZE, 0) == VM_UNMAPPED);

    good = user_vm_heap_alloc(vm, USER_HEAP_BASE, 0, -1) != VM_OK &&
           user_vm_heap_alloc(vm, USER_HEAP_BASE + 1, 1, -1) != VM_OK &&
           user_vm_heap_alloc(vm, USER_CODE_BASE, 1, -1) != VM_OK &&
           user_vm_heap_alloc(vm, USER_HEAP_LIMIT, 1, -1) != VM_OK &&
           user_vm_heap_alloc(vm, USER_HEAP_LIMIT - PAGE_SIZE, 2, -1) != VM_OK &&
           user_vm_heap_alloc(vm, USER_HEAP_BASE, ~0UL, -1) != VM_OK &&
           user_vm_heap_alloc(vm, USER_HEAP_BASE + 2 * PAGE_SIZE, 1, -2) != VM_OK &&
           user_vm_heap_alloc(0, USER_HEAP_BASE, 1, -1) != VM_OK &&
           user_vm_heap_free(vm, USER_HEAP_BASE, 0) != VM_OK &&
           user_vm_heap_free(vm, USER_HEAP_BASE + 1, 1) != VM_OK &&
           user_vm_heap_free(vm, USER_DATA_BASE, 1) != VM_OK &&
           user_vm_heap_free(vm, USER_HEAP_LIMIT, 1) != VM_OK &&
           user_vm_heap_free(vm, USER_HEAP_LIMIT - PAGE_SIZE, 2) != VM_OK &&
           user_vm_heap_free(vm, USER_HEAP_BASE, ~0UL) != VM_OK &&
           user_vm_heap_free(0, USER_HEAP_BASE, 1) != VM_OK;
    failures += check("heap-invalid-ranges-no-change", good && same_counts(live));

    /* VPN0 wraps here: three user frames require one new L0 table. Every
     * injected failure must preserve low live pages and both pool counts. */
    const uint64 edge = 0x1ff000UL;
    for (int budget = 0; budget <= 4; budget++) {
        result = user_vm_heap_alloc(vm, edge, 3, budget);
        good = budget < 4 ? result == VM_NO_MEMORY : result == VM_OK;
        if (result == VM_OK)
            good &= user_vm_heap_free(vm, edge, 3) == VM_OK;
        good &= same_counts(live) &&
                user_vm_lookup(vm, edge, 0) == VM_UNMAPPED &&
                user_vm_lookup(vm, edge + PAGE_SIZE, 0) == VM_UNMAPPED &&
                user_vm_lookup(vm, edge + 2 * PAGE_SIZE, 0) == VM_UNMAPPED &&
                has_mapping(vm, USER_HEAP_BASE, first.pa, VM_R | VM_W | VM_U);
        console_printf("[utest] memory-heap-budget=%d %s\n", budget, good ? "PASS" : "FAIL");
        failures += !good;
    }

    good = 1;
    for (unsigned round = 0; round < 8; round++) {
        if (user_vm_heap_alloc(vm, edge, 3, -1) != VM_OK) {
            good = 0;
            break;
        }
        uint64 allocated[2];
        free_counts(allocated);
        good &= allocated[PAGE_KERNEL] + 1 == live[PAGE_KERNEL] &&
                allocated[PAGE_USER] + 3 == live[PAGE_USER];
        good &= user_vm_heap_free(vm, edge, 3) == VM_OK && same_counts(live);
        good &= user_vm_heap_free(vm, edge, 3) == VM_UNMAPPED && same_counts(live);
    }
    failures += check("heap-cross-2m-repeated-table-reclaim", good);

    /* Keep allocations in separate L0 branches alive for destroy to own. */
    result = user_vm_heap_alloc(vm, 0x400000UL, 1, -1);
    uint64 remaining[2];
    free_counts(remaining);
    failures += check("heap-live-pages-await-destroy", result == VM_OK &&
                      remaining[PAGE_KERNEL] + 1 == baseline[PAGE_KERNEL] &&
                      remaining[PAGE_USER] + 4 == baseline[PAGE_USER]);
    return failures;
}

int user_memory_selftest(void)
{
    int failures = 0;
    uint64 initial[2], borrowed_baseline[2];
    free_counts(initial);
    void *frame = page_alloc(PAGE_KERNEL);
    void *stack = page_alloc(PAGE_KERNEL);
    if (!frame || !stack) {
        if (frame) page_free(PAGE_KERNEL, frame);
        if (stack) page_free(PAGE_KERNEL, stack);
        failures += check("borrowed-page-allocation", 0);
        failures += check("resource-balance", same_counts(initial));
        return failures;
    }
    memset(frame, 0x5a, PAGE_SIZE);
    memset(stack, 0xa5, PAGE_SIZE);
    free_counts(borrowed_baseline);

    struct user_vm vm = {0};
    int result = user_vm_create(&vm, _user_code_start,
                                (uint64)_user_code_end - (uint64)_user_code_start,
                                _user_data_start,
                                (uint64)_user_data_end - (uint64)_user_data_start,
                                (uint64)frame, (uint64)stack, -1);
    failures += check("production-create", result == VM_OK);
    if (result == VM_OK) {
        uint64 allocated[2];
        free_counts(allocated);
        failures += check("exact-owned-pages",
                          allocated[PAGE_KERNEL] + 5 == borrowed_baseline[PAGE_KERNEL] &&
                          allocated[PAGE_USER] + 3 == borrowed_baseline[PAGE_USER]);
        failures += check("code-rxu", has_mapping(&vm, USER_CODE_BASE, vm.code, VM_R | VM_X | VM_U));
        failures += check("data-rwu-nx", has_mapping(&vm, USER_DATA_BASE, vm.data, VM_R | VM_W | VM_U));
        failures += check("stack-rwu-nx", has_mapping(&vm, USER_STACK_BASE, vm.stack, VM_R | VM_W | VM_U));
        failures += check("trampoline-s-only",
                          has_mapping(&vm, TRAMPOLINE, (uint64)trampoline_start, VM_R | VM_X));
        failures += check("trapframe-s-only",
                          has_mapping(&vm, TRAPFRAME, (uint64)frame, VM_R | VM_W));
        failures += check("kernel-stack-s-only",
                          has_mapping(&vm, KERNEL_STACK_BASE, (uint64)stack, VM_R | VM_W));
        failures += check("null-unmapped", user_vm_lookup(&vm, 0, 0) == VM_UNMAPPED);
        failures += check("kernel-identity-unmapped", user_vm_lookup(&vm, 0x80200000UL, 0) == VM_UNMAPPED);
        failures += check("uart-unmapped", user_vm_lookup(&vm, 0x10000000UL, 0) == VM_UNMAPPED);
        failures += check("guard-unmapped", user_vm_lookup(&vm, USER_STACK_GUARD, 0) == VM_UNMAPPED);
        failures += check("heap-unmapped", user_vm_lookup(&vm, USER_HEAP_BASE, 0) == VM_UNMAPPED);

        /* We own these two frames. Modify their physical identity aliases,
         * then resolve a user span that genuinely crosses their boundary. */
        memcpy((uint8 *)vm.code + PAGE_SIZE - 4, "ABCD", 4);
        memcpy((void *)vm.data, "EFGH", 4);
        uint8 copied[8];
        memset(copied, 0xa5, sizeof copied);
        int good = user_vm_copyin(&vm, copied, USER_DATA_BASE - 4, sizeof copied) == VM_OK;
        const char expected[] = "ABCDEFGH";
        for (unsigned i = 0; i < sizeof copied; i++)
            if (copied[i] != (uint8)expected[i])
                good = 0;
        failures += check("valid-cross-page-copy", good);

        failures += check("bad-second-page-no-partial-copy",
                          rejected_without_copy(&vm, USER_HEAP_BASE - 4));
        failures += check("trapframe-copy-rejected", rejected_without_copy(&vm, TRAPFRAME));
        failures += check("trampoline-copy-rejected", rejected_without_copy(&vm, TRAMPOLINE));
        failures += check("kernel-stack-copy-rejected", rejected_without_copy(&vm, KERNEL_STACK_BASE));
        failures += check("wrap-copy-rejected", rejected_without_copy(&vm, ~0UL - 3));
        failures += check("zero-length-no-access", user_vm_copyin(0, 0, ~0UL, 0) == VM_OK);
        failures += heap_vm_tests(&vm, (uint64)frame);
    }

    user_vm_destroy(&vm);
    failures += check("destroy-restores-owned-pages", empty(&vm) && same_counts(borrowed_baseline));
    user_vm_destroy(&vm);
    failures += check("repeat-destroy-harmless", empty(&vm) && same_counts(borrowed_baseline));
    failures += check("borrowed-content-preserved",
                      bytes_are(frame, PAGE_SIZE, 0x5a) && bytes_are(stack, PAGE_SIZE, 0xa5));
    int freed_frame = page_free(PAGE_KERNEL, frame);
    int freed_stack = page_free(PAGE_KERNEL, stack);
    failures += check("borrowed-pages-remain-owned", freed_frame == PAGE_OK && freed_stack == PAGE_OK);
    failures += check("resource-balance", same_counts(initial));
    return failures;
}

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

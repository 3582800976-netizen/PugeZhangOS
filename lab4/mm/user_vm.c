/* An isolated Sv39 tree for one process. Unlike the kernel tree, this tree
 * contains no identity map: only three U pages and three borrowed S pages.
 * Creation is transactional. Even a half-built page-table branch belongs to
 * the local object and is traversed during rollback.
 */
#include "user_vm.h"
#include "user_layout.h"
#include "page.h"
#include "memory.h"

#define PTE_FLAGS 0x3ffUL
#define PTE_PPN (((1UL << 44) - 1) << 10)

extern char trampoline_start[];

static uint64 pte_address(uint64 entry)
{
    return ((entry & PTE_PPN) >> 10) << 12;
}

static uint64 pte_from_address(uint64 address)
{
    return (address >> 12) << 10;
}

static unsigned index_at(uint64 va, unsigned level)
{
    return (unsigned)((va >> (12 + 9 * level)) & 511);
}

static int in_kernel_pool(uint64 address)
{
    struct page_stats pool;
    page_get_stats(PAGE_KERNEL, &pool);
    return !(address & (PAGE_SIZE - 1)) &&
           address >= pool.base && address < pool.limit;
}

static void *allocate(enum page_pool pool, int *budget)
{
    if (*budget == 0)
        return 0;
    if (*budget > 0)
        --*budget;
    return page_alloc(pool);
}

/* This function is private: only the fixed layout in create can request a
 * user mapping. No caller can use it to expose kernel RAM with PTE_U. */
static int map_owned_tree(struct user_vm *vm, uint64 va, uint64 pa,
                          uint64 permissions, int *budget)
{
    if (va >= (uint64)USER_VA_MAX || ((va | pa) & (PAGE_SIZE - 1)) ||
        (pa >> 56) || !(permissions & VM_R) ||
        ((permissions & (VM_W | VM_X)) == (VM_W | VM_X)) ||
        (permissions & ~(uint64)(VM_R | VM_W | VM_X | VM_U)))
        return VM_INVALID_MAP;

    uint64 *table = (uint64 *)vm->root;
    for (int level = 2; level > 0; level--) {
        uint64 *entry = &table[index_at(va, (unsigned)level)];
        if (!(*entry & VM_V)) {
            void *next = allocate(PAGE_KERNEL, budget);
            if (!next)
                return VM_NO_MEMORY;
            *entry = pte_from_address((uint64)next) | VM_V;
        } else if (*entry & (VM_R | VM_W | VM_X)) {
            return VM_ALREADY_MAPPED;
        }
        table = (uint64 *)pte_address(*entry);
    }
    uint64 *entry = &table[index_at(va, 0)];
    if (*entry & VM_V)
        return VM_ALREADY_MAPPED;
    *entry = pte_from_address(pa) | permissions | VM_V | VM_A;
    if (permissions & VM_W)
        *entry |= VM_D;
    return VM_OK;
}

/* Free only page-table branches. Leaves include borrowed mappings, so leaf
 * PAs must never be returned to the allocator by this traversal. */
static void release_tree(uint64 address, unsigned level)
{
    uint64 *table = (uint64 *)address;
    if (level) {
        for (unsigned index = 0; index < 512; index++) {
            uint64 entry = table[index];
            if ((entry & VM_V) && !(entry & (VM_R | VM_W | VM_X)))
                release_tree(pte_address(entry), level - 1);
        }
    }
    page_free(PAGE_KERNEL, (void *)address);
}

void user_vm_destroy(struct user_vm *vm)
{
    if (!vm)
        return;
    if (vm->root)
        release_tree(vm->root, 2);
    if (vm->code)
        page_free(PAGE_USER, (void *)vm->code);
    if (vm->data)
        page_free(PAGE_USER, (void *)vm->data);
    if (vm->stack)
        page_free(PAGE_USER, (void *)vm->stack);
    memset(vm, 0, sizeof *vm);
}

int user_vm_create(struct user_vm *out, const void *code, uint64 code_size,
                   const void *data, uint64 data_size,
                   uint64 trapframe_pa, uint64 kstack_pa, int budget)
{
    if (!out)
        return VM_INVALID_MAP;
    memset(out, 0, sizeof *out);
    if (!code || !code_size || code_size > PAGE_SIZE || data_size > PAGE_SIZE ||
        (data_size && !data) || budget < -1 ||
        !in_kernel_pool(trapframe_pa) || !in_kernel_pool(kstack_pa) ||
        trapframe_pa == kstack_pa || ((uint64)trampoline_start & (PAGE_SIZE - 1)))
        return VM_INVALID_MAP;

    struct user_vm vm = {0};
    vm.root = (uint64)allocate(PAGE_KERNEL, &budget);
    if (!vm.root)
        return VM_NO_MEMORY;
    vm.code = (uint64)allocate(PAGE_USER, &budget);
    if (vm.code)
        vm.data = (uint64)allocate(PAGE_USER, &budget);
    if (vm.data)
        vm.stack = (uint64)allocate(PAGE_USER, &budget);
    int result = VM_NO_MEMORY;
    if (!vm.code || !vm.data || !vm.stack)
        goto rollback;
    memcpy((void *)vm.code, code, code_size);
    if (data_size)
        memcpy((void *)vm.data, data, data_size);

    const uint64 virtuals[] = {
        USER_CODE_BASE, USER_DATA_BASE, USER_STACK_BASE,
        TRAMPOLINE, TRAPFRAME, KERNEL_STACK_BASE
    };
    const uint64 physicals[] = {
        vm.code, vm.data, vm.stack,
        (uint64)trampoline_start, trapframe_pa, kstack_pa
    };
    const uint64 permissions[] = {
        VM_R | VM_X | VM_U, VM_R | VM_W | VM_U, VM_R | VM_W | VM_U,
        VM_R | VM_X, VM_R | VM_W, VM_R | VM_W
    };
    for (unsigned i = 0; i < sizeof virtuals / sizeof virtuals[0]; i++) {
        result = map_owned_tree(&vm, virtuals[i], physicals[i], permissions[i], &budget);
        if (result != VM_OK)
            goto rollback;
    }
    *out = vm;
    return VM_OK;

rollback:
    user_vm_destroy(&vm);
    return result;
}

int user_vm_lookup(const struct user_vm *vm, uint64 va, struct vm_mapping *out)
{
    /* Only the positive Sv39 half is part of this process ABI. In particular,
     * VA_MAX itself has bit38 set without sign extension and is invalid. */
    if (va >= (uint64)USER_VA_MAX)
        return VM_BAD_ADDRESS;
    if (!vm || !vm->root)
        return VM_UNMAPPED;
    if (!in_kernel_pool(vm->root))
        return VM_BAD_ENTRY;
    if (out) {
        memset(out, 0, sizeof *out);
        for (unsigned level = 0; level < 3; level++)
            out->vpn[level] = index_at(va, level);
    }
    uint64 *table = (uint64 *)vm->root;
    for (int level = 2; level >= 0; level--) {
        uint64 entry = table[index_at(va, (unsigned)level)];
        if (!(entry & VM_V))
            return VM_UNMAPPED;
        if ((entry >> 54) || ((entry & VM_W) && !(entry & VM_R)))
            return VM_BAD_ENTRY;
        uint64 address = pte_address(entry);
        if (entry & (VM_R | VM_X)) {
            /* This module creates 4KiB leaves exclusively. Reject a changed
             * superpage instead of silently weakening the fixed layout. */
            if (level != 0)
                return VM_BAD_ENTRY;
            if (out) {
                out->pa = address | (va & (PAGE_SIZE - 1));
                out->pte = entry;
                out->flags = entry & PTE_FLAGS;
                out->level = 0;
            }
            return VM_OK;
        }
        if (!level || (entry & (VM_U | VM_A | VM_D)) || !in_kernel_pool(address))
            return VM_BAD_ENTRY;
        table = (uint64 *)address;
    }
    return VM_UNMAPPED;
}

static int readable_user_page(const struct user_vm *vm, uint64 va, struct vm_mapping *m)
{
    int result = user_vm_lookup(vm, va, m);
    if (result != VM_OK)
        return result;
    if ((m->flags & (VM_U | VM_R)) != (VM_U | VM_R))
        return VM_INVALID_MAP;
    return VM_OK;
}

int user_vm_copyin(const struct user_vm *vm, void *dst, uint64 srcva, uint64 len)
{
    if (!len)
        return VM_OK;
    if (!dst || !vm || !vm->root || srcva >= (uint64)USER_VA_MAX ||
        len > (uint64)USER_VA_MAX - srcva)
        return VM_BAD_ADDRESS;

    /* First pass checks every touched page. A bad later page must not allow
     * even a partial WRITE or modification of the kernel destination. */
    struct vm_mapping mapping;
    for (uint64 checked = 0; checked < len;) {
        uint64 va = srcva + checked;
        int result = readable_user_page(vm, va, &mapping);
        if (result != VM_OK)
            return result;
        uint64 chunk = PAGE_SIZE - (va & (PAGE_SIZE - 1));
        if (chunk > len - checked)
            chunk = len - checked;
        checked += chunk;
    }

    /* The fixed process slot is owned by one hart; its page table cannot be
     * changed or destroyed between these passes. The kernel's identity map
     * lets us read the resolved PA without setting sstatus.SUM. */
    uint8 *bytes = dst;
    for (uint64 copied = 0; copied < len;) {
        uint64 va = srcva + copied;
        (void)user_vm_lookup(vm, va, &mapping);
        uint64 chunk = PAGE_SIZE - (va & (PAGE_SIZE - 1));
        if (chunk > len - copied)
            chunk = len - copied;
        memcpy(bytes + copied, (const void *)mapping.pa, chunk);
        copied += chunk;
    }
    return VM_OK;
}

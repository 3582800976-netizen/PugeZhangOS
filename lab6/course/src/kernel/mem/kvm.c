#include "mod.h"

// 内核页表只有在完整构造成功后才发布。
static pgtbl_t kernel_pgtbl;
extern char trampoline[];

// 映射事务在第一次新建中间页时才申请日志页。记录旧 PTE，使失败时连
// 非零但无效的旧条目也能恢复；不能通过删除所有空页表来代替回滚，
// 因为空页表可能是调用前已存在的。
struct vm_change {
    pte_t *slot;
    pte_t old;
    pgtbl_t page;
};
#define VM_CHANGES_PER_PAGE ((PGSIZE - 2 * sizeof(uint64)) / sizeof(struct vm_change))
struct vm_journal {
    struct vm_journal *previous;
    uint64 count;
    struct vm_change changes[VM_CHANGES_PER_PAGE];
};

static int journal_reserve(struct vm_journal **journal)
{
    if (*journal != NULL && (*journal)->count < VM_CHANGES_PER_PAGE)
        return 0;
    struct vm_journal *page = (struct vm_journal *)pmem_alloc(true);
    if (page == NULL)
        return -1;
    page->previous = *journal;
    page->count = 0;
    *journal = page;
    return 0;
}

static void journal_finish(struct vm_journal *journal, bool rollback)
{
    while (journal != NULL) {
        struct vm_journal *previous = journal->previous;
        if (rollback) {
            // 后建子表先撤销，保证 slot 指向的父表在恢复时仍然存在。
            while (journal->count != 0) {
                struct vm_change *change = &journal->changes[--journal->count];
                *change->slot = change->old;
                pmem_free((uint64)change->page, true);
            }
        }
        pmem_free((uint64)journal, true);
        journal = previous;
    }
}

// 单条路径最多创建两页；失败时恢复旧中间项，不遗留半条路径。
pte_t *vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc)
{
    if (pgtbl == NULL || va >= VA_MAX)
        return NULL;
    struct vm_change made[2];
    int count = 0;
    for (int level = 2; level > 0; level--) {
        pte_t *slot = &pgtbl[VA_TO_VPN(va, level)];
        if (*slot & PTE_V) {
            if (!PTE_CHECK(*slot)) {
                while (count) {
                    struct vm_change *change = &made[--count];
                    *change->slot = change->old;
                    pmem_free((uint64)change->page, true);
                }
                panic("vm_getpte: unexpected upper-level leaf");
                return NULL;
            }
            pgtbl = (pgtbl_t)PTE_TO_PA(*slot);
        } else {
            if (!alloc)
                return NULL;
            pgtbl_t page = (pgtbl_t)pmem_alloc(true);
            if (page == NULL) {
                while (count) {
                    struct vm_change *change = &made[--count];
                    *change->slot = change->old;
                    pmem_free((uint64)change->page, true);
                }
                return NULL;
            }
            made[count++] = (struct vm_change){slot, *slot, page};
            *slot = PA_TO_PTE(page) | PTE_V;
            pgtbl = page;
        }
    }
    return &pgtbl[VA_TO_VPN(va, 0)];
}

// 建立事务用的中间页；叶项不会在预检阶段被修改。
static pte_t *vm_prepare(pgtbl_t pgtbl, uint64 va,
                         struct vm_journal **journal, bool *bad_leaf)
{
    for (int level = 2; level > 0; level--) {
        pte_t *slot = &pgtbl[VA_TO_VPN(va, level)];
        if (*slot & PTE_V) {
            if (!PTE_CHECK(*slot)) {
                *bad_leaf = true;
                return NULL;
            }
            pgtbl = (pgtbl_t)PTE_TO_PA(*slot);
            continue;
        }
        if (journal_reserve(journal) != 0)
            return NULL;
        pgtbl_t page = (pgtbl_t)pmem_alloc(true);
        if (page == NULL)
            return NULL;
        struct vm_change *change = &(*journal)->changes[(*journal)->count++];
        *change = (struct vm_change){slot, *slot, page};
        *slot = PA_TO_PTE(page) | PTE_V;
        pgtbl = page;
    }
    return &pgtbl[VA_TO_VPN(va, 0)];
}

int vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm)
{
    const uint64 pa_limit = 1ULL << 56; // Sv39 PTE 中的 44 位物理页号
    if (pgtbl == NULL || (va & (PGSIZE - 1)) || (pa & (PGSIZE - 1)) ||
        len == 0 || len > ~0ULL - (PGSIZE - 1) || va >= VA_MAX ||
        pa >= pa_limit || (perm & ~(PTE_R | PTE_W | PTE_X | PTE_U)) ||
        !(perm & (PTE_R | PTE_X)) || ((perm & PTE_W) && !(perm & PTE_R)))
        return -1;
    uint64 rounded = (len + PGSIZE - 1) & ~((uint64)PGSIZE - 1);
    if (rounded > VA_MAX - va || rounded > pa_limit - pa)
        return -1;

    struct vm_journal *journal = NULL;
    for (uint64 offset = 0; offset < rounded; offset += PGSIZE) {
        bool bad_leaf = false;
        pte_t *slot = vm_prepare(pgtbl, va + offset, &journal, &bad_leaf);
        if (slot == NULL || (*slot & PTE_V)) {
            journal_finish(journal, true);
            if (bad_leaf)
                panic("vm_mappages: unexpected upper-level leaf");
            return -1;
        }
    }
    // 预检完成后写入叶项，不再申请资源，因此不会发生部分发布失败。
    for (uint64 offset = 0; offset < rounded; offset += PGSIZE) {
        pte_t *slot = vm_getpte(pgtbl, va + offset, false);
        *slot = PA_TO_PTE(pa + offset) | perm | PTE_V | PTE_MACHINE | PTE_A |
                ((perm & PTE_W) ? PTE_D : 0);
    }
    journal_finish(journal, false);
    sfence_vma();
    return 0;
}

void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit)
{
    assert(pgtbl != NULL && !(va & (PGSIZE - 1)) && len != 0 &&
           len <= ~0ULL - (PGSIZE - 1) && va < VA_MAX,
           "vm_unmappages: invalid range");
    uint64 rounded = (len + PGSIZE - 1) & ~((uint64)PGSIZE - 1);
    assert(rounded <= VA_MAX - va, "vm_unmappages: invalid range");
    for (uint64 offset = 0; offset < rounded; offset += PGSIZE) {
        pte_t *slot = vm_getpte(pgtbl, va + offset, false);
        assert(slot != NULL && (*slot & PTE_V) && !PTE_CHECK(*slot),
               "vm_unmappages: missing leaf");
    }
    for (uint64 offset = 0; offset < rounded; offset += PGSIZE) {
        pte_t *slot = vm_getpte(pgtbl, va + offset, false);
        uint64 page = PTE_TO_PA(*slot);
        *slot = 0;
        // 其他 hart 的失效由调用者按课程契约完成；本 hart 先失效再回收。
        sfence_vma();
        if (freeit)
            pmem_free(page, check_inkernel(page));
    }
}

void vm_freewalk(pgtbl_t pgtbl)
{
    if (pgtbl == NULL)
        return;
    for (int i = 0; i < (int)(PGSIZE / sizeof(pte_t)); i++) {
        pte_t entry = pgtbl[i];
        if ((entry & PTE_V) && PTE_CHECK(entry))
            vm_freewalk((pgtbl_t)PTE_TO_PA(entry));
    }
    // 叶项指向的页属于调用者；这里只归还页表页。
    pmem_free((uint64)pgtbl, true);
}

void kvm_init(void)
{
    pgtbl_t root = (pgtbl_t)pmem_alloc(true);
    uint64 stacks[N_PROC] = {0};
    if (root == NULL)
        panic("kvm_init: no root page");
    uint64 data = ((uint64)KERNEL_DATA + PGSIZE - 1) & ~((uint64)PGSIZE - 1);
    if (data < KERNEL_BASE || data > (uint64)ALLOC_END ||
        vm_mappages(root, UART_BASE, UART_BASE, PGSIZE, PTE_R | PTE_W) != 0 ||
        vm_mappages(root, PLIC_BASE, PLIC_BASE, PLIC_SIZE, PTE_R | PTE_W) != 0 ||
        vm_mappages(root, KERNEL_BASE, KERNEL_BASE, data - KERNEL_BASE, PTE_R | PTE_X) != 0 ||
        vm_mappages(root, data, data, (uint64)ALLOC_END - data, PTE_R | PTE_W) != 0 ||
        vm_mappages(root, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X) != 0)
        goto failed;
    for (int i = 0; i < N_PROC; i++) {
        stacks[i] = (uint64)pmem_alloc(true);
        if (!stacks[i] || vm_mappages(root, KSTACK(i), stacks[i], PGSIZE, PTE_R | PTE_W) != 0)
            goto failed;
    }
    kernel_pgtbl = root;
    return;

failed:
    for (int i = 0; i < N_PROC; i++)
        if (stacks[i])
            pmem_free(stacks[i], true);
    vm_freewalk(root);
    panic("kvm_init: page table or stack allocation failed");
}

void kvm_inithart(void)
{
    assert(kernel_pgtbl != NULL, "kvm_inithart: no page table");
    sfence_vma();
    w_satp(MAKE_SATP(kernel_pgtbl));
    sfence_vma();
}

static void vm_print_level(pgtbl_t table, int level)
{
    for (int i = 0; i < (int)(PGSIZE / sizeof(pte_t)); i++) {
        pte_t entry = table[i];
        if (!(entry & PTE_V))
            continue;
        printf("level=%d index=%d pte=%x pa=%x\n", level, i,
               (uint64)entry, (uint64)PTE_TO_PA(entry));
        if (level > 0 && PTE_CHECK(entry))
            vm_print_level((pgtbl_t)PTE_TO_PA(entry), level - 1);
    }
}

void vm_print(pgtbl_t pgtbl)
{
    assert(pgtbl != NULL, "vm_print: null table");
    vm_print_level(pgtbl, 2);
}

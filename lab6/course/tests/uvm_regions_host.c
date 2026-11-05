/* Deterministic host test of real UVM, KVM and node-pool algorithms.
 * The page allocator, spinlocks and privileged register operations are mocks. */
#include "src/kernel/mem/mod.h"
extern void _Exit(int status);

#define sfence_vma() ((void)0)
#define w_satp(value) ((void)(value))
#include "src/kernel/mem/kvm.c"
#include "src/kernel/mem/mmap.c"
#include "src/kernel/mem/uvm.c"

#define POOL_PAGES 512
static uint8 pages[POOL_PAGES][PGSIZE] __attribute__((aligned(PGSIZE)));
static int used[POOL_PAGES];
static bool is_kernel[POOL_PAGES];
static int live_pages;
static int kernel_budget = -1, user_budget = -1;
static proc_t current;

void panic(const char *message) { (void)message; _Exit(90); }
void assert(bool condition, const char *message) { if (!condition) panic(message); }
void spinlock_init(spinlock_t *lock, char *name) { (void)lock; (void)name; }
void spinlock_acquire(spinlock_t *lock) { (void)lock; }
void spinlock_release(spinlock_t *lock) { (void)lock; }
proc_t *myproc(void) { return &current; }
void memset(void *dst, uint8 value, uint32 count)
{
    uint8 *p = dst;
    while (count--) *p++ = value;
}
void memmove(void *dst, const void *src, uint32 count)
{
    uint8 *d = dst;
    const uint8 *s = src;
    if ((uint64)d < (uint64)s) for (uint32 i = 0; i < count; i++) d[i] = s[i];
    else while (count) { count--; d[count] = s[count]; }
}
static int page_index(uint64 page)
{
    for (int i = 0; i < POOL_PAGES; i++)
        if (page == (uint64)&pages[i][0]) return i;
    return -1;
}
void *pmem_alloc(bool kernel)
{
    int *budget = kernel ? &kernel_budget : &user_budget;
    if (*budget == 0) return NULL;
    for (int i = 0; i < POOL_PAGES; i++) {
        if (used[i]) continue;
        if (*budget > 0) (*budget)--;
        used[i] = 1; is_kernel[i] = kernel; live_pages++;
        memset(pages[i], 0, PGSIZE);
        return pages[i];
    }
    return NULL;
}
bool check_inkernel(uint64 page)
{
    int index = page_index(page);
    return index >= 0 && is_kernel[index];
}
void pmem_free(uint64 page, bool kernel)
{
    int index = page_index(page);
    if (index < 0 || !used[index] || is_kernel[index] != kernel) _Exit(91);
    used[index] = 0; live_pages--;
}

static int valid_user_page(pgtbl_t root, uint64 va)
{
    pte_t *pte = vm_getpte(root, va, false);
    return pte && (*pte & (PTE_V | PTE_U)) == (PTE_V | PTE_U) && !PTE_CHECK(*pte);
}
static uint8 *physical(pgtbl_t root, uint64 va)
{
    pte_t *pte = vm_getpte(root, va, false);
    if (!pte || !(*pte & PTE_V)) return NULL;
    return (uint8 *)(PTE_TO_PA(*pte) + (va & (PGSIZE - 1)));
}
static int setup(void)
{
    if (live_pages) return -1;
    memset(&current, 0, sizeof(current));
    mmap_init();
    pgtbl_t root = pmem_alloc(true);
    void *code = pmem_alloc(false), *stack = pmem_alloc(false);
    if (!root || !code || !stack ||
        vm_mappages(root, USER_BASE, (uint64)code, PGSIZE, PTE_U|PTE_R|PTE_W|PTE_X) ||
        vm_mappages(root, TRAPFRAME-PGSIZE, (uint64)stack, PGSIZE, PTE_U|PTE_R|PTE_W))
        return -1;
    current.pgtbl = root;
    current.heap_top = USER_BASE + PGSIZE;
    current.ustack_npage = 1;
    return 0;
}
static int cleanup(void)
{
    while (current.mmap) {
        mmap_region_t *next = current.mmap->next;
        mmap_region_free(current.mmap);
        current.mmap = next;
    }
    uvm_destroy_pgtbl(current.pgtbl);
    current.pgtbl = NULL;
    return live_pages;
}

static int test_heap_stack(void)
{
    if (setup()) return 1;
    int baseline = live_pages;
    uint64 base = current.heap_top;
    kernel_budget = 0;
    if (uvm_heap_grow(current.pgtbl, base, PGSIZE) != 0 ||
        live_pages != baseline || valid_user_page(current.pgtbl, base)) return 11;
    kernel_budget = -1;
    user_budget = 1;
    if (uvm_heap_grow(current.pgtbl, base, 2 * PGSIZE) != 0 ||
        live_pages != baseline || valid_user_page(current.pgtbl, base)) return 2;
    user_budget = -1;
    if (uvm_heap_grow(current.pgtbl, base, 1) != base + 1 ||
        !valid_user_page(current.pgtbl, base)) return 3;
    if (uvm_heap_grow(current.pgtbl, base + 1, PGSIZE - 1) != base + PGSIZE) return 4;
    if (uvm_heap_ungrow(current.pgtbl, base + PGSIZE, PGSIZE / 2) != base + PGSIZE / 2 ||
        !valid_user_page(current.pgtbl, base)) return 5;
    if (uvm_heap_ungrow(current.pgtbl, base + PGSIZE / 2, PGSIZE / 2) != base ||
        valid_user_page(current.pgtbl, base)) return 6;
    if (uvm_heap_ungrow(current.pgtbl, base, 1) != 0 ||
        uvm_heap_grow(current.pgtbl, MMAP_BEGIN, 1) != 0) return 7;
    baseline = live_pages;
    kernel_budget = 0;
    if (uvm_ustack_grow(current.pgtbl, 1, TRAPFRAME - 3 * PGSIZE + 5) != -1 ||
        live_pages != baseline || valid_user_page(current.pgtbl, TRAPFRAME - 2 * PGSIZE)) return 12;
    kernel_budget = -1;
    user_budget = 1;
    if (uvm_ustack_grow(current.pgtbl, 1, TRAPFRAME - 3 * PGSIZE + 5) != -1 ||
        live_pages != baseline || valid_user_page(current.pgtbl, TRAPFRAME - 2 * PGSIZE)) return 8;
    user_budget = -1;
    if (uvm_ustack_grow(current.pgtbl, 1, TRAPFRAME - 3 * PGSIZE + 5) != 3 ||
        !valid_user_page(current.pgtbl, TRAPFRAME - 3 * PGSIZE) ||
        !valid_user_page(current.pgtbl, TRAPFRAME - 2 * PGSIZE) ||
        uvm_ustack_grow(current.pgtbl, 3, TRAPFRAME - PGSIZE) != -1 ||
        uvm_ustack_grow(current.pgtbl, 3, MMAP_END - 1) != -1) return 9;
    return cleanup() ? 10 : 0;
}

static int test_mmap(void)
{
    if (setup()) return 1;
    int baseline = live_pages;
    for (int b = 0; b < 2; b++) {
        kernel_budget = b;
        if (uvm_mmap(0, 3, PTE_R|PTE_W) != 0 || current.mmap ||
            live_pages != baseline || valid_user_page(current.pgtbl, MMAP_BEGIN)) return 14;
    }
    kernel_budget = -1;
    user_budget = 1;
    if (uvm_mmap(0, 3, PTE_R|PTE_W) != 0 || current.mmap || live_pages != baseline ||
        valid_user_page(current.pgtbl, MMAP_BEGIN)) return 2;
    user_budget = -1;
    uint64 a = uvm_mmap(0, 2, PTE_R|PTE_W);
    if (a != MMAP_BEGIN || uvm_mmap(a + 3*PGSIZE, 2, PTE_R|PTE_W) != a + 3*PGSIZE ||
        uvm_mmap(a + 2*PGSIZE, 1, PTE_R|PTE_W) != a + 2*PGSIZE ||
        !current.mmap || current.mmap->npages != 5 || current.mmap->next) return 3;
    if (uvm_mmap(a + 5*PGSIZE, 2, PTE_R) != a + 5*PGSIZE ||
        !current.mmap->next || current.mmap->next->perm != (PTE_U|PTE_R) ||
        uvm_mmap(a + PGSIZE, 1, PTE_R|PTE_W) != 0 ||
        uvm_mmap(a + 8*PGSIZE, 1, PTE_W) != 0) return 4;
    if (uvm_munmap(a + 4*PGSIZE, 2) || current.mmap->npages != 4 ||
        current.mmap->next->begin != a + 6*PGSIZE || current.mmap->next->npages != 1) return 5;
    if (uvm_munmap(a + PGSIZE, 1) || current.mmap->npages != 1 ||
        current.mmap->next->begin != a + 2*PGSIZE || current.mmap->next->npages != 2) return 6;
    baseline = live_pages;
    if (uvm_munmap(a, 3) != -1 || live_pages != baseline ||
        !valid_user_page(current.pgtbl, a) || !valid_user_page(current.pgtbl, a + 2*PGSIZE)) return 7;
    if (uvm_munmap(a, 1) || uvm_munmap(a + 2*PGSIZE, 2) ||
        uvm_munmap(a + 6*PGSIZE, 1) || current.mmap) return 8;

    a = MMAP_BEGIN;
    if (uvm_mmap(a + 2*PGSIZE, 2, PTE_R|PTE_W) != a + 2*PGSIZE ||
        uvm_mmap(a + PGSIZE, 1, PTE_R|PTE_W) != a + PGSIZE ||
        !current.mmap || current.mmap->begin != a + PGSIZE ||
        current.mmap->npages != 3 || current.mmap->next ||
        uvm_munmap(a + PGSIZE, 3) || current.mmap) return 15;

    a = uvm_mmap(0, 3, PTE_R|PTE_W);
    if (!a) return 9;
    mmap_region_t *reserved[N_MMAP];
    int n = 0;
    while (n < N_MMAP && (reserved[n] = mmap_region_alloc()) != NULL) n++;
    if (n != N_MMAP - 1) return 10;
    baseline = live_pages;
    if (uvm_munmap(a + PGSIZE, 1) != -1 || live_pages != baseline ||
        current.mmap->npages != 3 || !valid_user_page(current.pgtbl, a + PGSIZE)) return 11;
    for (int i = 0; i < n; i++) mmap_region_free(reserved[i]);
    if (uvm_munmap(a + PGSIZE, 1) || uvm_munmap(a, 1) ||
        uvm_munmap(a + 2*PGSIZE, 1) || current.mmap) return 12;
    return cleanup() ? 13 : 0;
}

static int test_copy(void)
{
    if (setup()) return 1;
    uint64 top = uvm_heap_grow(current.pgtbl, current.heap_top, 2 * PGSIZE);
    if (!top) return 2;
    current.heap_top = top;
    if (!uvm_mmap(0, 3, PTE_R|PTE_W)) return 3;
    uint64 test_vas[] = {USER_BASE, 2*PGSIZE, 3*PGSIZE, MMAP_BEGIN,
                         MMAP_BEGIN+PGSIZE, MMAP_BEGIN+2*PGSIZE, TRAPFRAME-PGSIZE};
    for (unsigned i = 0; i < sizeof(test_vas)/sizeof(test_vas[0]); i++) {
        uint8 *p = physical(current.pgtbl, test_vas[i]);
        if (!p) return 4;
        *p = (uint8)(0x10 + i);
    }
    pgtbl_t child = pmem_alloc(true);
    void *trap = pmem_alloc(true), *tramp = pmem_alloc(true);
    if (!child || !trap || !tramp ||
        vm_mappages(child, TRAPFRAME, (uint64)trap, PGSIZE, PTE_R|PTE_W) ||
        vm_mappages(child, TRAMPOLINE, (uint64)tramp, PGSIZE, PTE_R|PTE_X)) return 5;
    pte_t *trap_pte = vm_getpte(child, TRAPFRAME, false);
    pte_t *tramp_pte = vm_getpte(child, TRAMPOLINE, false);
    pte_t *old_stack = vm_getpte(child, TRAPFRAME-PGSIZE, false);
    if (!trap_pte || !tramp_pte || !old_stack) return 6;
    pte_t saved_trap = *trap_pte, saved_tramp = *tramp_pte;
    *old_stack = 0x200; /* invalid, nonzero PTE must survive failed cloning */
    int baseline = live_pages;
    for (int b = 0; b < 7; b++) {
        user_budget = b;
        if (uvm_copy_pgtbl(current.pgtbl, child, current.heap_top,
                            current.ustack_npage, current.mmap) != -1 ||
            live_pages != baseline || *trap_pte != saved_trap || *tramp_pte != saved_tramp ||
            *old_stack != 0x200 || valid_user_page(child, USER_BASE) ||
            valid_user_page(child, MMAP_BEGIN)) return 7;
    }
    user_budget = -1;
    for (int b = 0; b < 4; b++) {
        kernel_budget = b;
        if (uvm_copy_pgtbl(current.pgtbl, child, current.heap_top,
                            current.ustack_npage, current.mmap) != -1 ||
            live_pages != baseline || *trap_pte != saved_trap || *tramp_pte != saved_tramp ||
            *old_stack != 0x200 || valid_user_page(child, USER_BASE)) return 8;
    }
    kernel_budget = -1;
    if (uvm_copy_pgtbl(current.pgtbl, child, current.heap_top,
                        current.ustack_npage, current.mmap)) return 9;
    for (unsigned i = 0; i < sizeof(test_vas)/sizeof(test_vas[0]); i++) {
        uint8 *src = physical(current.pgtbl, test_vas[i]);
        uint8 *dst = physical(child, test_vas[i]);
        pte_t *sp = vm_getpte(current.pgtbl, test_vas[i], false);
        pte_t *dp = vm_getpte(child, test_vas[i], false);
        if (!src || !dst || src == dst || *src != *dst ||
            ((*sp ^ *dp) & (PTE_R|PTE_W|PTE_X|PTE_U))) return 10;
        *dst ^= 0x80;
        if (*src == *dst) return 11;
    }
    if (*trap_pte != saved_trap || *tramp_pte != saved_tramp) return 12;
    uvm_destroy_pgtbl(child);
    pmem_free((uint64)trap, true);
    pmem_free((uint64)tramp, true);
    return cleanup() ? 13 : 0;
}

int main(void)
{
    int r = test_heap_stack(); if (r) return 10 + r;
    r = test_mmap(); if (r) return 30 + r;
    r = test_copy(); if (r) return 60 + r;
    return 0;
}

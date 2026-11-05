/* Host tests exercise kvm.c with a deterministic page pool; no hardware MMU. */
#include "src/kernel/mem/mod.h"
#include <setjmp.h>
extern void _Exit(int status);

/* Simulated aligned linker addresses; page-table pages still use the host pool. */
__asm__(".globl KERNEL_DATA\n.set KERNEL_DATA, 0x80400000\n"
        ".globl ALLOC_END\n.set ALLOC_END, 0x88200000\n"
        ".globl trampoline\n.set trampoline, 0x80240000\n");

/* These instructions require S-mode. The host test checks PTEs and ownership. */
#define sfence_vma() ((void)0)
#define w_satp(value) ((void)(value))
#include "src/kernel/mem/kvm.c"

#define POOL_PAGES 256
static uint8 pages[POOL_PAGES][PGSIZE] __attribute__((aligned(PGSIZE)));
static int used[POOL_PAGES];
static bool kernel_page[POOL_PAGES];
static int budget = -1;
static int live_pages;
static jmp_buf panic_target;
static int expect_panic;

void panic(const char *message)
{
    (void)message;
    if (expect_panic) longjmp(panic_target, 1);
    _Exit(90);
}
void assert(bool condition, const char *message) { if (!condition) panic(message); }

static int page_number(uint64 page)
{
    for (int i = 0; i < POOL_PAGES; i++)
        if (page == (uint64)&pages[i][0]) return i;
    return -1;
}
void *pmem_alloc(bool in_kernel)
{
    if (budget == 0) return NULL;
    for (int i = 0; i < POOL_PAGES; i++) {
        if (used[i]) continue;
        if (budget > 0) budget--;
        used[i] = 1;
        kernel_page[i] = in_kernel;
        live_pages++;
        for (int j = 0; j < PGSIZE; j++) pages[i][j] = 0;
        return pages[i];
    }
    return NULL;
}
bool check_inkernel(uint64 page)
{
    int i = page_number(page);
    return i >= 0 && kernel_page[i];
}
void pmem_free(uint64 page, bool in_kernel)
{
    int i = page_number(page);
    if (i < 0 || !used[i] || kernel_page[i] != in_kernel) _Exit(91);
    used[i] = 0;
    live_pages--;
}

static int test_getpte_rollback(void)
{
    pgtbl_t root = pmem_alloc(true);
    if (!root) return 1;
    for (int b = 0; b < 2; b++) {
        root[0] = 0x200; /* nonzero invalid PTE must survive a failed walk */
        budget = b;
        if (vm_getpte(root, 0, true) != NULL || root[0] != 0x200 || live_pages != 1)
            return 2;
    }
    budget = 2;
    if (!vm_getpte(root, 0, true) || live_pages != 3) return 3;
    budget = -1;
    vm_freewalk(root);
    return live_pages ? 4 : 0;
}

static int test_mapping_failures(void)
{
    pgtbl_t root = pmem_alloc(true);
    if (!root) return 1;
    root[0] = 0x200;
    for (int b = 0; b < 3; b++) {
        budget = b;
        if (vm_mappages(root, 0, 0x40000000, PGSIZE, PTE_R) != -1 ||
            root[0] != 0x200 || live_pages != 1) return 2;
    }
    budget = -1;
    if (vm_mappages(root, 0x200000, 0x40200000, PGSIZE, PTE_R | PTE_W)) return 3;
    pte_t before_root = root[0];
    pgtbl_t middle = (pgtbl_t)PTE_TO_PA(before_root);
    middle[0] = 0x400;
    pte_t before_other = middle[1];
    int live = live_pages;
    if (vm_mappages(root, 0, 0x40000000, 0x201000, PTE_R) != -1 ||
        root[0] != before_root || middle[0] != 0x400 ||
        middle[1] != before_other || live_pages != live) return 4;
    if (vm_mappages(root, 0x1000, 0x40001000, PGSIZE, PTE_W) != -1 ||
        vm_mappages(root, 0x1001, 0x40001000, PGSIZE, PTE_R) != -1 ||
        vm_mappages(root, VA_MAX - PGSIZE, 0x40001000, 2 * PGSIZE, PTE_R) != -1 ||
        vm_mappages(root, 0, 1ULL << 56, PGSIZE, PTE_R) != -1 ||
        vm_mappages(root, 0, 0x40000000, ~0ULL, PTE_R) != -1 ||
        live_pages != live) return 5;
    vm_freewalk(root);
    return live_pages ? 6 : 0;
}

static int test_multi_journal_rollback(void)
{
    pgtbl_t root = pmem_alloc(true);
    if (!root) return 1;
    for (int b = 171; b <= 172; b++) {
        root[0] = 0x200;
        budget = b;
        /* 170 distinct lowest-level tables require a second journal page. */
        if (vm_mappages(root, 0, 0x40000000, 170ULL * 0x200000, PTE_R) != -1 ||
            root[0] != 0x200 || live_pages != 1) return 2;
    }
    budget = -1;
    vm_freewalk(root);
    return live_pages ? 3 : 0;
}

static int test_unmap_ownership(void)
{
    pgtbl_t root = pmem_alloc(true);
    uint8 *user = pmem_alloc(false);
    uint8 *kernel = pmem_alloc(true);
    if (!root || !user || !kernel || kernel != user + PGSIZE) return 1;
    if (vm_mappages(root, 0x1000, (uint64)user, 2 * PGSIZE, PTE_R | PTE_W)) return 2;
    pte_t *first = vm_getpte(root, 0x1000, false);
    pte_t *second = vm_getpte(root, 0x2000, false);
    if (!first || !second || !(*first & PTE_A) || !(*first & PTE_D)) return 3;
    vm_unmappages(root, 0x1000, 2 * PGSIZE, true);
    if (*first || *second || used[page_number((uint64)user)] ||
        used[page_number((uint64)kernel)]) return 4;
    vm_freewalk(root);
    return live_pages ? 5 : 0;
}

static int test_freewalk_borrowed(void)
{
    pgtbl_t root = pmem_alloc(true);
    uint8 *frame = pmem_alloc(false);
    if (!root || !frame) return 1;
    frame[0] = 0x5a;
    if (vm_mappages(root, 0x3000, (uint64)frame, 1, PTE_R)) return 2;
    vm_freewalk(root);
    if (live_pages != 1 || !used[page_number((uint64)frame)] || frame[0] != 0x5a) return 3;
    pmem_free((uint64)frame, false);
    vm_freewalk(NULL);
    return live_pages ? 4 : 0;
}

static int test_kvm_init(void)
{
    /* Fail each early allocation in turn and require full rollback. */
    for (volatile int b = 0; b < 150; b++) {
        budget = b;
        expect_panic = 1;
        if (setjmp(panic_target) == 0) {
            kvm_init();
            /* The first successful budget finishes this loop. */
            expect_panic = 0;
            break;
        }
        expect_panic = 0;
        if (live_pages != 0 || kernel_pgtbl != NULL) return 1;
    }
    if (kernel_pgtbl == NULL) return 2;
    pte_t *uart = vm_getpte(kernel_pgtbl, UART_BASE, false);
    pte_t *plic_start = vm_getpte(kernel_pgtbl, PLIC_BASE, false);
    pte_t *plic_end = vm_getpte(kernel_pgtbl, PLIC_BASE + PLIC_SIZE - PGSIZE, false);
    pte_t *text = vm_getpte(kernel_pgtbl, KERNEL_BASE, false);
    pte_t *data = vm_getpte(kernel_pgtbl, 0x80400000, false);
    pte_t *last = vm_getpte(kernel_pgtbl, 0x88200000 - PGSIZE, false);
    pte_t *firmware = vm_getpte(kernel_pgtbl, 0x80000000, false);
    pte_t *clint = vm_getpte(kernel_pgtbl, 0x02000000, false);
    pte_t *tramp = vm_getpte(kernel_pgtbl, TRAMPOLINE, false);
    if (!uart || PTE_TO_PA(*uart) != UART_BASE ||
        (*uart & (PTE_V|PTE_R|PTE_W|PTE_X|PTE_U)) != (PTE_V|PTE_R|PTE_W) ||
        !plic_start || !(*plic_start & PTE_V) || !plic_end || !(*plic_end & PTE_V) ||
        !text || PTE_TO_PA(*text) != KERNEL_BASE ||
        (*text & (PTE_V|PTE_R|PTE_W|PTE_X|PTE_U)) != (PTE_V|PTE_R|PTE_X) ||
        !data || (*data & (PTE_V|PTE_R|PTE_W|PTE_X|PTE_U)) != (PTE_V|PTE_R|PTE_W) ||
        !last || !(*last & PTE_V) || (firmware && (*firmware & PTE_V)) ||
        (clint && (*clint & PTE_V)) || !tramp || PTE_TO_PA(*tramp) != 0x80240000 ||
        (*tramp & (PTE_V|PTE_R|PTE_W|PTE_X|PTE_U)) != (PTE_V|PTE_R|PTE_X)) return 3;
    for (int i = 0; i < N_PROC; i++) {
        pte_t *stack = vm_getpte(kernel_pgtbl, KSTACK(i), false);
        pte_t *guard = vm_getpte(kernel_pgtbl, KSTACK(i) + PGSIZE, false);
        if (!stack || (*stack & (PTE_V|PTE_R|PTE_W|PTE_X|PTE_U)) != (PTE_V|PTE_R|PTE_W) ||
            (guard && (*guard & PTE_V))) return 4;
        for (int j = 0; j < i; j++) {
            pte_t *other = vm_getpte(kernel_pgtbl, KSTACK(j), false);
            if (PTE_TO_PA(*other) == PTE_TO_PA(*stack)) return 5;
        }
    }
    for (int i = 0; i < N_PROC; i++) {
        pte_t *stack = vm_getpte(kernel_pgtbl, KSTACK(i), false);
        pmem_free(PTE_TO_PA(*stack), true);
    }
    vm_freewalk(kernel_pgtbl);
    kernel_pgtbl = NULL;
    budget = -1;
    return live_pages ? 6 : 0;
}

int main(void)
{
    int result = test_getpte_rollback();
    if (result) return 10 + result;
    result = test_mapping_failures();
    if (result) return 20 + result;
    result = test_multi_journal_rollback();
    if (result) return 60 + result;
    result = test_unmap_ownership();
    if (result) return 30 + result;
    result = test_freewalk_borrowed();
    if (result) return 40 + result;
    result = test_kvm_init();
    if (result) return 50 + result;
    return 0;
}

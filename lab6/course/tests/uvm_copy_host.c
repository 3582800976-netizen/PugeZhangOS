/* Host probe for user-copy permission and cross-page behavior only. */
#include "src/kernel/mem/mod.h"
extern int posix_memalign(void **, unsigned long, unsigned long);
extern void free(void *);
extern void _Exit(int);

static pte_t ptes[2];
static unsigned char *first_page, *second_page;
static uint64 returned[4];
static bool returned_kernel[4];
static unsigned returned_count;

pte_t *vm_getpte(pgtbl_t table, uint64 va, bool alloc)
{
    (void)table; (void)alloc;
    if (va >= PGSIZE && va < PGSIZE * 3ULL)
        return &ptes[(va / PGSIZE) - 1];
    return NULL;
}
void memmove(void *dst, const void *src, uint32 n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (uint32 i = 0; i < n; i++) d[i] = s[i];
}
void assert(bool condition, const char *message)
{
    (void)message;
    if (!condition) _Exit(90);
}
void pmem_free(uint64 page, bool kernel)
{
    if (returned_count >= 4) _Exit(91);
    returned[returned_count] = page;
    returned_kernel[returned_count++] = kernel;
}
int main(void)
{
    if (posix_memalign((void **)&first_page, PGSIZE, PGSIZE) ||
        posix_memalign((void **)&second_page, PGSIZE, PGSIZE)) return 1;
    ptes[0] = PA_TO_PTE(first_page) | PTE_V | PTE_U | PTE_R | PTE_W;
    ptes[1] = PA_TO_PTE(second_page) | PTE_V | PTE_U | PTE_R | PTE_W;
    pgtbl_t table = ptes;
    const uint64 crossing = 2ULL * PGSIZE - 2;
    char input[] = {'a', 'b', 'c', '\0'};
    char output[4] = {0};
    if (uvm_copyout(table, crossing, (uint64)input, 4) != 0) return 2;
    if (first_page[PGSIZE-2] != 'a' || first_page[PGSIZE-1] != 'b' ||
        second_page[0] != 'c' || second_page[1] != 0) return 3;
    if (uvm_copyin(table, (uint64)output, crossing, 4) != 0 ||
        output[0] != 'a' || output[1] != 'b' || output[2] != 'c' || output[3] != 0) return 4;
    if (uvm_copyin_str(table, (uint64)output, crossing, 4) != 0 ||
        uvm_copyin_str(table, (uint64)output, crossing, 3) != -1) return 5;
    ptes[1] &= ~PTE_W;
    if (uvm_copyout(table, crossing, (uint64)input, 4) != -1) return 6;
    ptes[0] &= ~PTE_U;
    if (uvm_copyin(table, (uint64)output, crossing, 4) != -1) return 7;
    if (uvm_copyin(table, (uint64)output, TRAPFRAME, 1) != -1) return 8;
    if (uvm_copyin(table, (uint64)output, crossing, 0) != 0) return 9;
    pte_t *root, *middle, *leaf;
    if (posix_memalign((void **)&root, PGSIZE, PGSIZE) ||
        posix_memalign((void **)&middle, PGSIZE, PGSIZE) ||
        posix_memalign((void **)&leaf, PGSIZE, PGSIZE)) return 10;
    for (unsigned i = 0; i < PGSIZE / sizeof(pte_t); i++)
        root[i] = middle[i] = leaf[i] = 0;
    root[0] = PA_TO_PTE(middle) | PTE_V;
    middle[0] = PA_TO_PTE(leaf) | PTE_V;
    leaf[1] = PA_TO_PTE(first_page) | PTE_V | PTE_U | PTE_R;
    leaf[2] = PA_TO_PTE(second_page) | PTE_V | PTE_R;
    uvm_destroy_pgtbl(root);
    if (returned_count != 4 || returned[0] != (uint64)first_page || returned_kernel[0] ||
        returned[1] != (uint64)leaf || !returned_kernel[1] ||
        returned[2] != (uint64)middle || !returned_kernel[2] ||
        returned[3] != (uint64)root || !returned_kernel[3]) return 11;
    free(root);
    free(middle);
    free(leaf);
    free(first_page);
    free(second_page);
    return 0;
}

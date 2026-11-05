/* Host test for page-pool ownership; does not emulate the hardware MMU. */
#include "src/kernel/mem/mod.h"
extern void _Exit(int);
#define TEST_PAGES (KERN_PAGES + 8)
__attribute__((aligned(PGSIZE))) unsigned char fake_ram[TEST_PAGES * PGSIZE];
__asm__(".globl ALLOC_BEGIN\n.set ALLOC_BEGIN,fake_ram\n"
        ".globl ALLOC_END\n.set ALLOC_END,fake_ram+4227072\n");
uint64 boot_dtb;
void spinlock_init(spinlock_t *lock, char *name) { (void)lock; (void)name; }
void spinlock_acquire(spinlock_t *lock) { (void)lock; }
void spinlock_release(spinlock_t *lock) { (void)lock; }
void panic(const char *message) { (void)message; _Exit(91); }
void assert(bool condition, const char *message) { if (!condition) panic(message); }
void memset(void *dst, uint8 byte, uint32 n)
{
    uint8 *p = dst;
    for (uint32 i = 0; i < n; i++) p[i] = byte;
}
int main(void)
{
    void *pages[8];
    pmem_init();
    if (!check_inkernel((uint64)fake_ram) ||
        check_inkernel((uint64)fake_ram + (uint64)KERN_PAGES * PGSIZE)) return 7;
    for (int i = 0; i < 8; i++) {
        pages[i] = pmem_alloc(false);
        if (!pages[i] || (uint64)pages[i] % PGSIZE) return 1;
        for (int j = 0; j < i; j++) if (pages[j] == pages[i]) return 2;
        uint8 *p = pages[i];
        for (uint32 j = 0; j < PGSIZE; j++) if (p[j] != 0) return 3;
        p[42] = 0xaa;
    }
    if (pmem_alloc(false) != NULL) return 4;
    pmem_free((uint64)pages[0], false);
    if (((uint8 *)pages[0])[42] != 1) return 5;
    void *again = pmem_alloc(false);
    if (again != pages[0] || ((uint8 *)again)[42] != 0) return 6;
    return 0;
}

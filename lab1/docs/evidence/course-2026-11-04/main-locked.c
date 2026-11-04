#include "arch/mod.h"
#include "lib/mod.h"

static int started;
static int ready;
static int finished;
static int sum;
static spinlock_t sum_lock;
static spinlock_t outer[2];
static spinlock_t inner[2];

int main(void)
{
    int hart = (int)r_tp();
    if (hart == 0) {
        print_init();
        spinlock_init(&sum_lock, "test sum");
        printf("[format] %d %p %x %c %s %%\n", -2147483647 - 1,
               (uint32)0xffffffffU, (uint64)0x12, 'Q', (char *)0);
        __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
    } else {
        while (!__atomic_load_n(&started, __ATOMIC_ACQUIRE)) ;
    }
    __atomic_fetch_add(&ready, 1, __ATOMIC_ACQ_REL);
    while (__atomic_load_n(&ready, __ATOMIC_ACQUIRE) != NCPU) ;
    int errors = 0;
    spinlock_init(&outer[hart], "outer");
    spinlock_init(&inner[hart], "inner");
    if (spinlock_holding(&outer[hart])) errors++;
    intr_on();
    if (!intr_get()) errors++;
    spinlock_acquire(&outer[hart]);
    if (intr_get() || !spinlock_holding(&outer[hart])) errors++;
    spinlock_acquire(&inner[hart]);
    if (intr_get() || !spinlock_holding(&inner[hart])) errors++;
    spinlock_release(&inner[hart]);
    if (intr_get() || spinlock_holding(&inner[hart])) errors++;
    spinlock_release(&outer[hart]);
    if (!intr_get() || spinlock_holding(&outer[hart])) errors++;
    intr_off();
    spinlock_acquire(&outer[hart]);
    spinlock_release(&outer[hart]);
    if (intr_get()) errors++;
    printf("[lock-test] hart=%d errors=%d\n", hart, errors);
    for (int i = 0; i < 20; i++)
        printf("[line] hart=%d index=%d AAAABBBBCCCCDDDDEEEE\n", hart, i);
    for (int i = 0; i < 1000000; i++) {
        spinlock_acquire(&sum_lock);
        sum++;
        spinlock_release(&sum_lock);
    }
    __atomic_fetch_add(&finished, 1, __ATOMIC_ACQ_REL);
    if (hart == 0) {
        while (__atomic_load_n(&finished, __ATOMIC_ACQUIRE) != NCPU) ;
        printf("[sum] %d\n", sum);
        printf("[done]\n");
    }
    for (;;) asm volatile("wfi");
}

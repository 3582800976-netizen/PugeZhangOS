/* Lab 1 中设备中断始终关闭；这里只处理多个核心之间的竞争。 */
#include "spinlock.h"

void spin_init(struct spinlock *lock)
{
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELAXED);
}

void spin_acquire(struct spinlock *lock)
{
    /* 交换返回旧值：只有旧值为 0 的核心成功得到锁。 */
    while (__atomic_exchange_n(&lock->locked, 1, __ATOMIC_ACQUIRE))
        asm volatile("nop");
}

void spin_release(struct spinlock *lock)
{
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE);
}

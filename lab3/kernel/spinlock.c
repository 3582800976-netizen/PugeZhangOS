#include "spinlock.h"
#include "platform.h"
#include "riscv.h"
#include "console.h"

// Each hart nests IRQ masking independently, so locks can safely nest.
static uint32 irq_depth[MAX_HARTS];
static uint32 irq_was_enabled[MAX_HARTS];

void irq_push(void)
{
    uint64 status;
    asm volatile("csrrc %0, sstatus, %1" : "=r"(status) : "r"(2UL) : "memory");
    uint64 id = r_tp();
    if (irq_depth[id] == 0) irq_was_enabled[id] = (status & 2) != 0;
    irq_depth[id]++;
}

void irq_pop(void)
{
    uint64 id = r_tp();
    if (!irq_depth[id]) panic("unbalanced interrupt mask");
    irq_depth[id]--;
    if (!irq_depth[id] && irq_was_enabled[id])
        asm volatile("csrs sstatus, %0" : : "r"(2UL) : "memory");
}

void spin_init(struct spinlock *lock)
{
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELAXED);
}

void spin_acquire(struct spinlock *lock)
{
    irq_push();
    while (__atomic_exchange_n(&lock->locked, 1, __ATOMIC_ACQUIRE))
        asm volatile("nop");
}

void spin_release(struct spinlock *lock)
{
    __atomic_store_n(&lock->locked, 0, __ATOMIC_RELEASE);
    irq_pop();
}

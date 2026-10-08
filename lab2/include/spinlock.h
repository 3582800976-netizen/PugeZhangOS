#ifndef PG_SPINLOCK_H
#define PG_SPINLOCK_H
#include "types.h"
struct spinlock { uint32 locked; };
void spin_init(struct spinlock *lock);
void spin_acquire(struct spinlock *lock);
void spin_release(struct spinlock *lock);
void irq_push(void);
void irq_pop(void);
#endif

#ifndef PUGE_SPINLOCK_H
#define PUGE_SPINLOCK_H
#include "types.h"
struct spinlock { uint32 locked; };
void spin_init(struct spinlock *lock);
void spin_acquire(struct spinlock *lock);
void spin_release(struct spinlock *lock);
#endif

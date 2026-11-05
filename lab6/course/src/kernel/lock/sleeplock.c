#include "mod.h"
#include "../proc/method.h"

// 睡眠锁初始化
void sleeplock_init(sleeplock_t *lk, char *name)
{
    spinlock_init(&lk->lock, "sleeplock");
    lk->locked = 0;
    lk->name = name;
    lk->pid = 0;
}

// 检查当前进程是否持有睡眠锁
bool sleeplock_holding(sleeplock_t *lk)
{
    spinlock_acquire(&lk->lock);
    proc_t *p = myproc();
    bool held = p != NULL && lk->locked && lk->pid == p->pid;
    spinlock_release(&lk->lock);
    return held;
}

// 当前进程尝试获取睡眠锁, 失败进入睡眠状态
void sleeplock_acquire(sleeplock_t *lk)
{
    spinlock_acquire(&lk->lock);
    proc_t *p = myproc();
    assert(p != NULL, "sleeplock_acquire: no process");
    assert(!lk->locked || lk->pid != p->pid, "sleeplock_acquire: recursive");
    while (lk->locked)
        proc_sleep(lk, &lk->lock);
    lk->locked = 1;
    lk->pid = p->pid;
    spinlock_release(&lk->lock);
}

// 释放睡眠锁, 唤醒其他等待睡眠锁的进程
void sleeplock_release(sleeplock_t *lk)
{
    spinlock_acquire(&lk->lock);
    proc_t *p = myproc();
    assert(p != NULL && lk->locked && lk->pid == p->pid,
           "sleeplock_release: not owner");
    lk->locked = 0;
    lk->pid = 0;
    proc_wakeup(lk);
    spinlock_release(&lk->lock);
}

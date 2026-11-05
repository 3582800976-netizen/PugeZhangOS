#include "mod.h"
#include "initcode.h" // Makefile 提供按平台隔离的 target/<platform>/user

#define initcode target_user_initcode
#define initcode_len target_user_initcode_len

// in trampoline.S
extern char trampoline[];

// in swtch.S
extern void swtch(context_t *old, context_t *new);

// in trap/trap_user.c
extern void trap_user_return();

/* ------------本地变量----------- */

// 进程结构体数组 + 第一个用户进程的指针
static proc_t proc_list[N_PROC];
static proc_t *proczero;

// 全局pid + 保护它的锁
static int global_pid;
static spinlock_t pid_lk;
// 保护父子关系与 wait 的“检查条件 → 入睡”交接。
static spinlock_t wait_lk;
static void proc_try_wakeup(proc_t *p);

/* 获取一个pid */
static int alloc_pid()
{
    int tmp = 0;
    spinlock_acquire(&pid_lk);
    if (global_pid <= 0) {
        spinlock_release(&pid_lk);
        return -1;
    }
    tmp = global_pid;
    global_pid = global_pid == 2147483647 ? 0 : global_pid + 1;
    spinlock_release(&pid_lk);
    return tmp;
}

/* 释放进程锁 + trap_user_return */
static void proc_return()
{
    proc_t *p = myproc();
    assert(p != NULL && spinlock_holding(&p->lk), "proc_return: no process lock");
    spinlock_release(&p->lk);
    trap_user_return();
    panic("proc_return: returned from user entry");
}

/* 进程模块初始化 */
void proc_init()
{
    spinlock_init(&pid_lk, "pid");
    spinlock_init(&wait_lk, "wait");
    global_pid = 1;
    proczero = NULL;
    for (int i = 0; i < N_PROC; i++) {
        spinlock_init(&proc_list[i].lk, "proc");
        proc_list[i].state = UNUSED;
        proc_list[i].kstack = KSTACK(i);
    }
}

/*
    申请一个UNUSED进程结构体(返回时带锁)
    并执行通用的初始化逻辑
*/
proc_t *proc_alloc()
{
    for (int i = 0; i < N_PROC; i++) {
        proc_t *p = &proc_list[i];
        spinlock_acquire(&p->lk);
        if (p->state != UNUSED) {
            spinlock_release(&p->lk);
            continue;
        }
        p->state = EMBRYO;
        p->pid = alloc_pid();
        p->tf = NULL;
        p->pgtbl = NULL;
        p->mmap = NULL;
        p->heap_top = 0;
        p->ustack_npage = 0;
        p->sleep_space = NULL;
        p->exit_code = 0;
        p->name[0] = '\0';
        if (p->pid < 0)
            goto failed;
        p->tf = pmem_alloc(true);
        if (p->tf == NULL)
            goto failed;
        p->pgtbl = proc_pgtbl_init((uint64)p->tf);
        if (p->pgtbl == NULL)
            goto failed;
        memset(&p->ctx, 0, sizeof(p->ctx));
        p->ctx.ra = (uint64)proc_return;
        p->ctx.sp = p->kstack + PGSIZE;
        return p; // 调用者持有进程锁，直到完成所有资源并发布 RUNNABLE。
failed:
        proc_free(p);
        spinlock_release(&p->lk);
        return NULL;
    }
    return NULL;
}

/*
    回收一个进程结构体并释放它包含的资源
    tips: 调用者需要持有进程锁
*/
void proc_free(proc_t *p)
{
    assert(p != NULL && spinlock_holding(&p->lk), "proc_free: no process lock");
    assert(p->state != RUNNING, "proc_free: process is running");
    if (p->pgtbl != NULL) {
        uvm_destroy_pgtbl(p->pgtbl);
        p->pgtbl = NULL;
    }
    if (p->tf != NULL) {
        pmem_free((uint64)p->tf, true);
        p->tf = NULL;
    }
    while (p->mmap != NULL) {
        mmap_region_t *next = p->mmap->next;
        mmap_region_free(p->mmap);
        p->mmap = next;
    }
    if (p->parent != NULL) {
        assert(spinlock_holding(&wait_lk), "proc_free: no parent lock");
        p->parent = NULL;
    }
    p->pid = 0;
    p->name[0] = '\0';
    p->heap_top = 0;
    p->ustack_npage = 0;
    p->sleep_space = NULL;
    p->exit_code = 0;
    memset(&p->ctx, 0, sizeof(p->ctx));
    p->state = UNUSED; // 内核栈属于槽位，保留供下次复用。
}

/*
    获得一个初始化过的用户页表
    完成trapframe和trampoline的映射
*/
pgtbl_t proc_pgtbl_init(uint64 trapframe)
{
    pgtbl_t pgtbl = pmem_alloc(true);
    if (pgtbl == NULL)
        return NULL;
    // 两处均为借用映射；失败时只回收页表结构，不释放借用的物理页。
    if (vm_mappages(pgtbl, TRAMPOLINE, (uint64)trampoline,
                    PGSIZE, PTE_R | PTE_X) < 0 ||
        vm_mappages(pgtbl, TRAPFRAME, trapframe,
                    PGSIZE, PTE_R | PTE_W) < 0) {
        vm_freewalk(pgtbl);
        return NULL;
    }
    return pgtbl;
}

/*
    第一个用户态进程的创建
    它的代码和数据位于initcode.h的initcode数组

    第一个进程的用户地址空间布局:
    trapoline   (1 page)
    trapframe   (1 page)
    ustack      (1 page)
    .......
                        <--heap_top
    code + data (1 page)
    empty space (1 page) 最低的4096字节 不分配物理页，同时不可访问
*/
void proc_make_first()
{
    proc_t *p = proc_alloc();
    if (p == NULL)
        panic("proc_make_first: no process slot");
    if (initcode_len == 0 || initcode_len > PGSIZE)
        goto failed;
    void *code = pmem_alloc(false);
    if (code == NULL)
        goto failed;
    // 课程首进程使用只读可执行代码页；可写数据使用堆或栈。
    if (vm_mappages(p->pgtbl, USER_BASE, (uint64)code, PGSIZE,
                    PTE_U | PTE_R | PTE_X) < 0) {
        pmem_free((uint64)code, false);
        goto failed;
    }
    memmove(code, initcode, initcode_len);
    void *stack = pmem_alloc(false);
    if (stack == NULL)
        goto failed;
    if (vm_mappages(p->pgtbl, TRAPFRAME - PGSIZE, (uint64)stack,
                    PGSIZE, PTE_U | PTE_R | PTE_W) < 0) {
        pmem_free((uint64)stack, false);
        goto failed;
    }
    p->heap_top = USER_BASE + PGSIZE;
    p->ustack_npage = 1;
    p->tf->user_to_kern_epc = USER_BASE;
    p->tf->sp = TRAPFRAME;
    memmove(p->name, "init", 5);
    proczero = p;
    p->state = RUNNABLE;
    spinlock_release(&p->lk);
    return;
failed:
    proc_free(p);
    spinlock_release(&p->lk);
    panic("proc_make_first: resource failure");
}

/*
    父进程产生子进程
    UNUSED -> RUNNABLE
*/
int proc_fork()
{
    proc_t *parent = myproc();
    assert(parent != NULL, "proc_fork: no current process");
    proc_t *child = proc_alloc();
    if (child == NULL)
        return -1;
    child->heap_top = parent->heap_top;
    child->ustack_npage = parent->ustack_npage;
    mmap_region_t **tail = &child->mmap;
    for (mmap_region_t *region = parent->mmap; region != NULL; region = region->next) {
        mmap_region_t *copy = mmap_region_alloc();
        if (copy == NULL)
            goto failed;
        copy->begin = region->begin;
        copy->npages = region->npages;
        copy->perm = region->perm;
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    if (uvm_copy_pgtbl(parent->pgtbl, child->pgtbl, parent->heap_top,
                       parent->ustack_npage, parent->mmap) < 0)
        goto failed;
    memmove(child->tf, parent->tf, sizeof(*child->tf));
    child->tf->a0 = 0; // 子进程看到 fork 返回 0。
    memmove(child->name, parent->name, sizeof(child->name));
    int pid = child->pid;

    // 状态为 EMBRYO 时其他核心不会运行它；按 wait_lk → 进程锁顺序发布。
    spinlock_release(&child->lk);
    spinlock_acquire(&wait_lk);
    spinlock_acquire(&child->lk);
    child->parent = parent;
    child->state = RUNNABLE;
    spinlock_release(&child->lk);
    spinlock_release(&wait_lk);
    return pid;
failed:
    proc_free(child);
    spinlock_release(&child->lk);
    return -1;
}

/*
    进程主动放弃CPU控制权
    RUNNING->RUNNABLE
*/
void proc_yield()
{
    proc_t *p = myproc();
    assert(p != NULL, "proc_yield: no current process");
    spinlock_acquire(&p->lk);
    assert(p->state == RUNNING, "proc_yield: not running");
    p->state = RUNNABLE;
    proc_sched();
    spinlock_release(&p->lk);
}

/*
    当父进程退出时, 让它的所有子进程认proczero为父
    因为proczero永不退出, 可以回收子进程的资源
*/
static void proc_reparent(proc_t *parent)
{
    // 调用者持有 wait_lk；首进程退出时没有其他收养者。
    if (parent == proczero || proczero == NULL)
        return;
    bool adopted = false;
    for (int i = 0; i < N_PROC; i++) {
        if (proc_list[i].parent == parent) {
            proc_list[i].parent = proczero;
            adopted = true;
        }
    }
    if (adopted) {
        spinlock_acquire(&proczero->lk);
        proc_try_wakeup(proczero);
        spinlock_release(&proczero->lk);
    }
}

/*
    唤醒等待呼叫的进程
    由proc_exit调用
    tips: 调用者需要持有p的进程锁
*/
static void proc_try_wakeup(proc_t *p)
{
    assert(spinlock_holding(&p->lk), "proc_try_wakeup: no process lock");
    if (p->state == SLEEPING && p->sleep_space == p)
        p->state = RUNNABLE;
}

/*
    进程退出
    RUNNING -> ZOMBIE
*/
void proc_exit(int exit_code)
{
    proc_t *p = myproc();
    assert(p != NULL, "proc_exit: no current process");
    spinlock_acquire(&wait_lk);
    proc_reparent(p);
    if (p->parent != NULL) {
        spinlock_acquire(&p->parent->lk);
        proc_try_wakeup(p->parent);
        spinlock_release(&p->parent->lk);
    }
    spinlock_acquire(&p->lk);
    p->exit_code = exit_code;
    p->state = ZOMBIE;
    spinlock_release(&wait_lk);
    proc_sched();
    panic("proc_exit: zombie resumed");
}

/*
    父进程等待一个子进程进入ZOMBIE状态
    1. 如果等到: 释放子进程, 返回子进程的pid, 将子进程的退出状态传出到user_addr
    2. 如果发现没孩子: 返回-1
    3. 如果没等到: 父进程进入睡眠状态
*/
int proc_wait(uint64 user_addr)
{
    proc_t *parent = myproc();
    assert(parent != NULL, "proc_wait: no current process");
    spinlock_acquire(&wait_lk);
    for (;;) {
        bool have_child = false;
        for (int i = 0; i < N_PROC; i++) {
            proc_t *child = &proc_list[i];
            if (child->parent != parent)
                continue;
            spinlock_acquire(&child->lk);
            have_child = true;
            if (child->state == ZOMBIE) {
                int pid = child->pid;
                // 复制失败时保留 ZOMBIE，父进程可以用合法地址重试。
                if (user_addr != 0 &&
                    uvm_copyout(parent->pgtbl, user_addr,
                                (uint64)&child->exit_code,
                                sizeof(child->exit_code)) < 0) {
                    spinlock_release(&child->lk);
                    spinlock_release(&wait_lk);
                    return -1;
                }
                proc_free(child);
                spinlock_release(&child->lk);
                spinlock_release(&wait_lk);
                return pid;
            }
            spinlock_release(&child->lk);
        }
        if (!have_child) {
            spinlock_release(&wait_lk);
            return -1;
        }
        // 与 child exit 使用相同 wait_lk 完成检查条件到睡眠的交接。
        proc_sleep(parent, &wait_lk);
    }
}

/*
    进程等待sleep_space对应的资源, 进入睡眠状态
    RUNNING -> SLEEPING
*/
void proc_sleep(void *sleep_space, spinlock_t *lock)
{
    proc_t *p = myproc();
    assert(p != NULL && lock != NULL, "proc_sleep: invalid context");
    assert(spinlock_holding(lock), "proc_sleep: condition lock not held");
    // 先拿到进程锁，再放开条件锁；唤醒方不会错过睡眠状态。
    if (lock != &p->lk) {
        spinlock_acquire(&p->lk);
        spinlock_release(lock);
    }
    p->sleep_space = sleep_space;
    p->state = SLEEPING;
    proc_sched();
    p->sleep_space = NULL;
    if (lock != &p->lk) {
        spinlock_release(&p->lk);
        spinlock_acquire(lock);
    }
}

/*
    唤醒所有等待sleep_space的进程
    SLEEPING -> RUNNABLE
*/
void proc_wakeup(void *sleep_space)
{
    // 逐槽持锁，不能同时持有多个进程锁；当前进程不可能正在睡眠。
    push_off();
    proc_t *current = myproc();
    for (int i = 0; i < N_PROC; i++) {
        proc_t *p = &proc_list[i];
        if (p == current)
            continue;
        spinlock_acquire(&p->lk);
        if (p->state == SLEEPING && p->sleep_space == sleep_space)
            p->state = RUNNABLE;
        spinlock_release(&p->lk);
    }
    pop_off();
}

/*
    用户进程切换到调度器
    tips: 调用者保证持有当前进程的锁
*/
void proc_sched()
{
    proc_t *p = myproc();
    assert(p != NULL && spinlock_holding(&p->lk), "proc_sched: no process lock");
    assert(mycpu()->noff == 1 && !intr_get(), "proc_sched: invalid interrupt state");
    assert(p->state != RUNNING, "proc_sched: process still running");
    int old_origin = mycpu()->origin;
    swtch(&p->ctx, &mycpu()->ctx);
    // 进程恢复时可能在另一个 hart；恢复当前 hart 的中断记账。
    mycpu()->origin = old_origin;
}

/*
    调度器
    RUNNABLE->RUNNING
*/
void proc_scheduler()
{
    cpu_t *cpu = mycpu();
    cpu->proc = NULL;
    for (;;) {
        intr_on();
        for (int i = 0; i < N_PROC; i++) {
            proc_t *p = &proc_list[i];
            spinlock_acquire(&p->lk);
            if (p->state == RUNNABLE) {
                p->state = RUNNING;
                cpu->proc = p;
                // 进程锁跨 swtch 交接；切回后由调度器释放。
                swtch(&cpu->ctx, &p->ctx);
                cpu->proc = NULL;
            }
            spinlock_release(&p->lk);
        }
    }
}

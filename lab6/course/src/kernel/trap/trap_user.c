#include "mod.h"
#include "../proc/method.h"

// in trampoline.S
extern char trampoline[];  // 内核和用户切换的代码
extern char user_vector[]; // 用户触发陷阱进入内核
extern char user_return[]; // 内核处理完毕返回用户

// in trap.S
extern char kernel_vector[]; // 内核态trap处理流程, 进入内核后应当切换中断处理入口

// in trap_kernel.c
extern char *interrupt_info[16]; // 中断错误信息
extern char *exception_info[16]; // 异常错误信息

// 在user_vector()里面调用
// 用户态trap处理的核心逻辑
void trap_user_handler()
{
    assert(!(r_sstatus() & SSTATUS_SPP), "trap_user_handler: not user mode");
    // 处理用户陷入时先换回内核入口，之后允许内核响应中断。
    w_stvec((uint64)kernel_vector);
    proc_t *p = myproc();
    assert(p != NULL && p->tf != NULL, "trap_user_handler: no process");
    p->tf->user_to_kern_epc = r_sepc();
    uint64 cause = r_scause();
    bool timer = false;
    if (cause == 8) {
        // ecall 总是 4 字节；异常本身记录的是 ecall 的地址。
        p->tf->user_to_kern_epc += 4;
        intr_on();
        syscall();
    } else if (cause == ((1ULL << 63) | 5)) {
        timer_interrupt_handler();
        timer = true;
    } else if (cause == ((1ULL << 63) | 9)) {
        external_interrupt_handler();
    } else if (cause == 13 || cause == 15) {
        int64 pages = uvm_ustack_grow(p->pgtbl, p->ustack_npage, r_stval());
        if (pages < 0)
            proc_exit(-1);
        p->ustack_npage = (uint64)pages;
    } else {
        proc_exit(-1);
    }
    if (timer)
        proc_yield();
    trap_user_return();
}

// 调用user_return()
// 内核态返回用户态
void trap_user_return()
{
    intr_off();
    proc_t *p = myproc();
    assert(p != NULL && p->tf != NULL && p->pgtbl != NULL,
           "trap_user_return: no process context");
    w_stvec(TRAMPOLINE + ((uint64)user_vector - (uint64)trampoline));
    p->tf->user_to_kern_satp = r_satp();
    p->tf->user_to_kern_sp = p->kstack + PGSIZE;
    p->tf->user_to_kern_trapvector = (uint64)trap_user_handler;
    p->tf->user_to_kern_hartid = r_tp();
    uint64 status = r_sstatus();
    status &= ~SSTATUS_SPP;
    status |= SSTATUS_SPIE;
    w_sstatus(status);
    w_sepc(p->tf->user_to_kern_epc);
    uint64 user_satp = MAKE_SATP(p->pgtbl);
    uint64 entry = TRAMPOLINE + ((uint64)user_return - (uint64)trampoline);
    ((void (*)(uint64, uint64))entry)(TRAPFRAME, user_satp);
    panic("trap_user_return: returned from trampoline");
}

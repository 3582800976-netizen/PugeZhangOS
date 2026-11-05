#include "mod.h"

// in trampoline.S
extern char trampoline[];  // 内核和用户切换的代码
extern char user_vector[]; // 用户触发陷阱进入内核
extern char user_return[]; // 内核处理完毕返回用户

// in trap.S
extern char kernel_vector[]; // 内核态trap处理流程, 进入内核后应当切换中断处理入口

// in trap_kernel.c
extern char *interrupt_info[16]; // 中断错误信息
extern char *exception_info[16]; // 异常错误信息
extern void swtch(context_t *old, context_t *new);

// 在user_vector()里面调用
// 用户态trap处理的核心逻辑
void trap_user_handler()
{
    assert(!(r_sstatus() & SSTATUS_SPP), "trap_user_handler: not user mode");
    w_stvec((uint64)kernel_vector);
    proc_t *p = myproc();
    assert(p != NULL && p->tf != NULL, "trap_user_handler: no process");
    p->tf->user_to_kern_epc = r_sepc();
    uint64 cause = r_scause();
    if (cause == 8) {
        p->tf->user_to_kern_epc += 4;
        syscall();
    } else if (cause == ((1ULL << 63) | 5)) {
        timer_interrupt_handler();
    } else if (cause == ((1ULL << 63) | 9)) {
        external_interrupt_handler();
    } else if (cause == 13 || cause == 15) {
        int64 pages = uvm_ustack_grow(p->pgtbl, p->ustack_npage, r_stval());
        if (pages >= 0) {
            p->ustack_npage = (uint64)pages;
            trap_user_return();
        }
        printf("user fault: cause=%x epc=%x address=%x\n", cause,
               p->tf->user_to_kern_epc, r_stval());
        intr_off();
        swtch(&p->ctx, &mycpu()->ctx);
        panic("user fault resumed");
    } else {
        printf("user fault: cause=%x epc=%x address=%x\n", cause,
               p->tf->user_to_kern_epc, r_stval());
        intr_off();
        swtch(&p->ctx, &mycpu()->ctx);
        panic("user fault resumed");
    }
    trap_user_return();
}

// 调用user_return()
// 内核态返回用户态
void trap_user_return()
{
    intr_off();
    proc_t *p = myproc();
    assert(p != NULL && p->tf != NULL, "trap_user_return: no process");
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
    uint64 target = TRAMPOLINE + ((uint64)user_return - (uint64)trampoline);
    ((void (*)(uint64, uint64))target)(TRAPFRAME, MAKE_SATP(p->pgtbl));
    panic("trap_user_return: returned");
}

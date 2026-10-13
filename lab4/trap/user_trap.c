/* U-origin traps use a protected frame; kernel-origin traps keep old path. */
#include "process.h"
#include "trap.h"
#include "syscall.h"
#include "riscv.h"
#include "user_layout.h"
#include "vm.h"
#include "console.h"

#define SSTATUS_SUM (1UL << 18)
#define SSTATUS_MXR (1UL << 19)
extern void kernel_vector(void);
extern char trampoline_start[], user_vector[], user_return[];

void user_trap(struct user_frame *frame)
{
    /* Hardware cleared SIE. The assembly already restored kernel satp/sp/tp. */
    uint64 cause = r_scause(), value = r_stval();
    w_stvec((uint64)kernel_vector);
    w_sscratch(0);
    struct process *p = process_current();
    if (!p || frame != p->tf || intr_get() || (frame->status & SSTATUS_SPP))
        panic("user trap lacks trusted U-origin context");
    p->result.user_origin = 1;
    if (cause & (1UL << 63)) {
        if (cause == ((1UL << 63) | 5)) ++p->result.user_timer_interrupts;
        if (cause == ((1UL << 63) | 9)) ++p->result.user_external_interrupts;
        frame->epc = trap_handle_interrupt(cause, frame->epc);
    } else if (cause == 8) {
        ++p->result.syscalls;
        frame->epc += 4;          /* ecall is always the 32-bit instruction */
        syscall_dispatch(frame);
    } else {
        p->result.cause = cause;
        p->result.epc = frame->epc;
        p->result.value = value;
        process_finish(PROCESS_FAULTED, -1);
    }
    trap_user_return();
}

void trap_user_return(void)
{
    struct process *p = process_current();
    if (!p || intr_get()) panic("invalid user return context");
    struct user_frame *frame = p->tf;
    frame->kernel_satp = SATP_SV39 | (vm_root_address() >> 12);
    frame->kernel_sp = p->kstack;
    frame->kernel_handler = (uint64)user_trap;
    frame->kernel_hartid = r_tp();
    uint64 status = r_sstatus();
    status &= ~(SSTATUS_SPP | SSTATUS_SIE | SSTATUS_SUM | SSTATUS_MXR);
    status |= SSTATUS_SPIE;
    w_sstatus(status);
    w_sepc(frame->epc);
    w_sscratch(TRAPFRAME);
    w_stvec(TRAMPOLINE + ((uint64)user_vector - (uint64)trampoline_start));
    uint64 target = TRAMPOLINE + ((uint64)user_return - (uint64)trampoline_start);
    void (*bridge)(uint64, uint64) = (void (*)(uint64, uint64))target;
    bridge(TRAPFRAME, SATP_SV39 | (p->pgtbl >> 12));
    __builtin_unreachable();
}

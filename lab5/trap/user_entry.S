/* The same RX supervisor-only page is mapped in kernel and user tables.
 * No calls or low-address memory references are allowed in this bridge.
 */
#include "user_frame.h"
    .section .trampoline,"ax",@progbits
    .balign 4096
    .globl trampoline_start
trampoline_start:
    .balign 4
    .globl user_vector
user_vector:
    /* Obtain a trusted frame address without losing the user's a0. */
    csrrw a0, sscratch, a0
    sd zero, 0(a0)
    sd ra, 8(a0)
    sd sp, 16(a0)
    sd gp, 24(a0)
    sd tp, 32(a0)
    sd t0, 40(a0)
    sd t1, 48(a0)
    sd t2, 56(a0)
    sd s0, 64(a0)
    sd s1, 72(a0)
    sd a1, 88(a0)
    sd a2, 96(a0)
    sd a3, 104(a0)
    sd a4, 112(a0)
    sd a5, 120(a0)
    sd a6, 128(a0)
    sd a7, 136(a0)
    sd s2, 144(a0)
    sd s3, 152(a0)
    sd s4, 160(a0)
    sd s5, 168(a0)
    sd s6, 176(a0)
    sd s7, 184(a0)
    sd s8, 192(a0)
    sd s9, 200(a0)
    sd s10, 208(a0)
    sd s11, 216(a0)
    sd t3, 224(a0)
    sd t4, 232(a0)
    sd t5, 240(a0)
    sd t6, 248(a0)
    csrr t0, sscratch
    sd t0, 80(a0)
    csrr t0, sepc
    sd t0, UF_EPC(a0)
    csrr t0, sstatus
    sd t0, UF_STATUS(a0)
    ld t0, UF_KERNEL_SATP(a0)
    ld sp, UF_KERNEL_SP(a0)
    ld t1, UF_KERNEL_HANDLER(a0)
    ld tp, UF_KERNEL_HART(a0)    /* never use the user's tp as hart identity */
    csrw satp, t0
    sfence.vma zero, zero
    jr t1                     /* kernel SATP is now active; a0 remains frame VA */

    .balign 4
    .globl user_return
user_return:
    /* C passes a0=frame VA, a1=user satp and already set sepc/sstatus. */
    csrw sscratch, a0
    csrw satp, a1
    sfence.vma zero, zero
    fence.i
    ld ra, 8(a0)
    ld sp, 16(a0)
    ld gp, 24(a0)
    ld tp, 32(a0)
    ld t0, 40(a0)
    ld t1, 48(a0)
    ld t2, 56(a0)
    ld s0, 64(a0)
    ld s1, 72(a0)
    ld a1, 88(a0)
    ld a2, 96(a0)
    ld a3, 104(a0)
    ld a4, 112(a0)
    ld a5, 120(a0)
    ld a6, 128(a0)
    ld a7, 136(a0)
    ld s2, 144(a0)
    ld s3, 152(a0)
    ld s4, 160(a0)
    ld s5, 168(a0)
    ld s6, 176(a0)
    ld s7, 184(a0)
    ld s8, 192(a0)
    ld s9, 200(a0)
    ld s10, 208(a0)
    ld s11, 216(a0)
    ld t3, 224(a0)
    ld t4, 232(a0)
    ld t5, 240(a0)
    ld t6, 248(a0)
    ld a0, 80(a0)               /* restore frame base register last */
    sret
    .globl trampoline_end
trampoline_end:

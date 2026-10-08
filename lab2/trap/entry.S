/* Lab 2 page-fault entry: preserve all integer registers on this hart's stack.
 * Hardware saves PC/status and clears SIE; it does NOT save integer registers.
 * 272-byte frame = 31 x-register slots + sepc + sstatus + alignment padding.
 */
    .section .text
    .balign 4
    .globl kernel_vector
kernel_vector:
    addi sp, sp, -272
    sd ra, 0(sp)
    sd gp, 16(sp)
    sd tp, 24(sp)
    sd t0, 32(sp)
    sd t1, 40(sp)
    sd t2, 48(sp)
    sd s0, 56(sp)
    sd s1, 64(sp)
    sd a0, 72(sp)
    sd a1, 80(sp)
    sd a2, 88(sp)
    sd a3, 96(sp)
    sd a4, 104(sp)
    sd a5, 112(sp)
    sd a6, 120(sp)
    sd a7, 128(sp)
    sd s2, 136(sp)
    sd s3, 144(sp)
    sd s4, 152(sp)
    sd s5, 160(sp)
    sd s6, 168(sp)
    sd s7, 176(sp)
    sd s8, 184(sp)
    sd s9, 192(sp)
    sd s10, 200(sp)
    sd s11, 208(sp)
    sd t3, 216(sp)
    sd t4, 224(sp)
    sd t5, 232(sp)
    sd t6, 240(sp)
    addi t0, sp, 272
    sd t0, 8(sp)                  /* interrupted stack pointer */
    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
    sd a1, 248(sp)
    csrr t0, sstatus
    sd t0, 256(sp)
    call trap_handle             /* returns the continuation PC */
    csrw sepc, a0
    ld t0, 256(sp)
    csrw sstatus, t0
    ld ra, 0(sp)
    ld gp, 16(sp)
    ld tp, 24(sp)
    ld t0, 32(sp)
    ld t1, 40(sp)
    ld t2, 48(sp)
    ld s0, 56(sp)
    ld s1, 64(sp)
    ld a0, 72(sp)
    ld a1, 80(sp)
    ld a2, 88(sp)
    ld a3, 96(sp)
    ld a4, 104(sp)
    ld a5, 112(sp)
    ld a6, 120(sp)
    ld a7, 128(sp)
    ld s2, 136(sp)
    ld s3, 144(sp)
    ld s4, 152(sp)
    ld s5, 160(sp)
    ld s6, 168(sp)
    ld s7, 176(sp)
    ld s8, 184(sp)
    ld s9, 192(sp)
    ld s10, 200(sp)
    ld s11, 208(sp)
    ld t3, 216(sp)
    ld t4, 224(sp)
    ld t5, 232(sp)
    ld t6, 240(sp)
    ld sp, 8(sp)
    sret

/* Permission probes recover only deliberately armed faults at exact PCs. */
    .balign 4
    .globl trap_load_probe
trap_load_probe:
    .option push
    .option norvc
    .globl trap_load_fault_site
trap_load_fault_site:
    lbu t0, 0(a0)
    .option pop
    li a0, 0
    ret
    .globl trap_load_fault_resume
trap_load_fault_resume:
    li a0, 1
    ret

    .balign 4
    .globl trap_store_probe
trap_store_probe:
    lbu a1, 0(a0)                 /* same byte: harmless if write is permitted */
    .option push
    .option norvc
    .globl trap_store_fault_site
trap_store_fault_site:
    sb a1, 0(a0)
    .option pop
    li a0, 0
    ret
    .globl trap_store_fault_resume
trap_store_fault_resume:
    li a0, 1
    ret

/* GETPID must preserve every user register except the a0 return value. */
#include "syscall.h"
    .section .text
    .balign 4
    .globl user_register_probe
user_register_probe:
    addi sp, sp, -160
    sd ra, 0(sp)
    sd gp, 8(sp)
    sd tp, 16(sp)
    sd s0, 24(sp)
    sd s1, 32(sp)
    sd s2, 40(sp)
    sd s3, 48(sp)
    sd s4, 56(sp)
    sd s5, 64(sp)
    sd s6, 72(sp)
    sd s7, 80(sp)
    sd s8, 88(sp)
    sd s9, 96(sp)
    sd s10, 104(sp)
    sd s11, 112(sp)
    sd a0, 120(sp)              /* expected PID */
    sd sp, 128(sp)              /* this stack pointer must survive */
    li ra, 0x31
    li gp, 0x33
    li tp, 0xfeed34
    li s0, 0x50
    li s1, 0x51
    li s2, 0x52
    li s3, 0x53
    li s4, 0x54
    li s5, 0x55
    li s6, 0x56
    li s7, 0x57
    li s8, 0x58
    li s9, 0x59
    li s10, 0x5a
    li s11, 0x5b
    li t0, 0x70
    li t1, 0x71
    li t2, 0x72
    li t3, 0x73
    li t4, 0x74
    li t5, 0x75
    li t6, 0x76
    li a1, 0x41
    li a2, 0x42
    li a3, 0x43
    li a4, 0x44
    li a5, 0x45
    li a6, 0x46
    li a7, 0x1
    li a0, 0x40
    ecall
    sd t0, 136(sp)              /* check original t0 after using it as scratch */
    ld t0, 120(sp)
    bne a0, t0, .Lfail
    ld t0, 128(sp)
    bne sp, t0, .Lfail
    li t0, 0x31
    bne ra, t0, .Lfail
    li t0, 0x33
    bne gp, t0, .Lfail
    li t0, 0xfeed34
    bne tp, t0, .Lfail
    li t0, 0x50
    bne s0, t0, .Lfail
    li t0, 0x51
    bne s1, t0, .Lfail
    li t0, 0x52
    bne s2, t0, .Lfail
    li t0, 0x53
    bne s3, t0, .Lfail
    li t0, 0x54
    bne s4, t0, .Lfail
    li t0, 0x55
    bne s5, t0, .Lfail
    li t0, 0x56
    bne s6, t0, .Lfail
    li t0, 0x57
    bne s7, t0, .Lfail
    li t0, 0x58
    bne s8, t0, .Lfail
    li t0, 0x59
    bne s9, t0, .Lfail
    li t0, 0x5a
    bne s10, t0, .Lfail
    li t0, 0x5b
    bne s11, t0, .Lfail
    li t0, 0x71
    bne t1, t0, .Lfail
    li t0, 0x72
    bne t2, t0, .Lfail
    li t0, 0x73
    bne t3, t0, .Lfail
    li t0, 0x74
    bne t4, t0, .Lfail
    li t0, 0x75
    bne t5, t0, .Lfail
    li t0, 0x76
    bne t6, t0, .Lfail
    li t0, 0x41
    bne a1, t0, .Lfail
    li t0, 0x42
    bne a2, t0, .Lfail
    li t0, 0x43
    bne a3, t0, .Lfail
    li t0, 0x44
    bne a4, t0, .Lfail
    li t0, 0x45
    bne a5, t0, .Lfail
    li t0, 0x46
    bne a6, t0, .Lfail
    li t0, 0x1
    bne a7, t0, .Lfail
    ld t0, 136(sp)
    addi t0, t0, -112
    bnez t0, .Lfail
    li a0, 1
    j .Lrestore
.Lfail:
    li a0, 0
.Lrestore:
    ld ra, 0(sp)
    ld gp, 8(sp)
    ld tp, 16(sp)
    ld s0, 24(sp)
    ld s1, 32(sp)
    ld s2, 40(sp)
    ld s3, 48(sp)
    ld s4, 56(sp)
    ld s5, 64(sp)
    ld s6, 72(sp)
    ld s7, 80(sp)
    ld s8, 88(sp)
    ld s9, 96(sp)
    ld s10, 104(sp)
    ld s11, 112(sp)
    addi sp, sp, 160
    ret

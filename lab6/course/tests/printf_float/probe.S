.section .rodata
probe_fmt: .asciz "probe=%f next=%d\n"
.section .text
.globl float_register_probe
float_register_probe:
    addi sp, sp, -16
    sd ra, 8(sp)
    li t0, 0x400921fb54442d18
    .irp reg, f0,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,f20,f21,f22,f23,f24,f25,f26,f27,f28,f29,f30,f31
      fmv.d.x \reg, t0
    .endr
    la a0, probe_fmt
    li a1, 0x3ff8000000000000
    li a2, 42
    call printf
    li t0, 0x400921fb54442d18
    .irp reg, f0,f1,f2,f3,f4,f5,f6,f7,f8,f9,f10,f11,f12,f13,f14,f15,f16,f17,f18,f19,f20,f21,f22,f23,f24,f25,f26,f27,f28,f29,f30,f31
      fmv.x.d t1, \reg
      bne t0, t1, 1f
    .endr
    li a0, 1
    j 2f
1:  li a0, 0
2:  ld ra, 8(sp)
    addi sp, sp, 16
    ret

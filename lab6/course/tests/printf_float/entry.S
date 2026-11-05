.section .text.entry
.globl _start
_start:
    la sp, stack_top
    li t0, 0x6000
    csrs mstatus, t0
    call main
1:  wfi
    j 1b
.section .bss
.balign 16
stack: .space 16384
stack_top:

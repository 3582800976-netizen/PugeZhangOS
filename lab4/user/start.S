/* First user instruction is at 0x1000. The kernel supplies mode in a0. */
#include "syscall.h"
    .section .text.entry
    .balign 4
    .globl _user_start
_user_start:
    la gp, __global_pointer$
    li tp, 0xfeed                 /* deliberately unrelated to a hart ID */
    la t0, _user_bss_start
    la t1, _user_bss_end
.Lzero:
    bgeu t0, t1, .Lmain
    sb zero, 0(t0)
    addi t0, t0, 1
    j .Lzero
.Lmain:
    call first_main
    li a7, SYS_EXIT
    ecall
.Lhalt:
    j .Lhalt

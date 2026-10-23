/* C ABI: user_syscall3(number, x, y, z). Kernel ABI: a7, a0, a1, a2. */
.section .text
.globl user_syscall3
user_syscall3:
    mv a7, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3
    ecall
    ret

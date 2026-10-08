/* Lab 1 只诊断致命异常，不恢复执行，也不处理设备中断。
 * 因为处理函数永不返回，此处无需保存供恢复使用的通用寄存器。
 * 完整的寄存器保存、恢复和 sret 放在 Lab 3 中学习。
 */
    .section .text
    .balign 4
    .globl kernel_vector
kernel_vector:
    csrr a0, scause
    csrr a1, sepc
    csrr a2, stval
    call trap_handle
.Lhalt:
    wfi
    j .Lhalt

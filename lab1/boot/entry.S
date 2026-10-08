/* Lab 1：QEMU -bios none，CPU 从 M 模式进入本文件。 */
#include "platform.h"
    .section .text.entry
    .balign 4
    .globl _boot
_boot:
    csrw mie, zero               /* 暂时禁止机器模式中断 */
    csrr a0, mhartid             /* 第一个 C 参数：当前核心编号 */
    li t0, CPUS
    bgeu a0, t0, .Lpark         /* 必须先检查编号，再索引每核数组 */
    mv tp, a0                  /* tp 保存编号，供 C 代码读取 */

    /* 多个核心同时到达；原子操作选出一个负责清零 BSS 的核心。
     * 这两个标志位放 .data，避免清零 BSS 时把标志也擦掉。
     */
    la t0, boot_claim
    li t1, 1
    amoswap.w.aq t2, t1, (t0)
    seqz a1, t2                /* 第二个 C 参数：是否负责全局初始化 */
    beqz a1, .Lwait_bss
    la t0, _bss_start
    la t1, _bss_end
.Lzero:
    bgeu t0, t1, .Lpublish
    sd zero, 0(t0)
    addi t0, t0, 8
    j .Lzero
.Lpublish:
    la t0, boot_ready
    li t1, 1
    fence rw, w                /* 先完成清零，再公布完成标志 */
    sw t1, 0(t0)
    j .Lstack
.Lwait_bss:
    la t0, boot_ready
.Lwait:
    lw t1, 0(t0)
    beqz t1, .Lwait
    fence r, rw                /* 看到标志后，才能使用已清零的变量 */
.Lstack:
    la sp, _boot_stack_top
    li t0, BOOT_STACK_SIZE
    mul t0, t0, a0
    sub sp, sp, t0             /* 各核心占用独立的 4096 字节栈 */
    call boot_start
.Lpark:
    wfi
    j .Lpark

    .section .data.boot,"aw",@progbits
    .balign 4
boot_claim: .word 0
boot_ready: .word 0

    .section .bss.stacks,"aw",@nobits
    .balign 4096
    .globl _boot_stack_base
_boot_stack_base:
    .space BOOT_STACK_SIZE * MAX_HARTS
    .globl _boot_stack_top
_boot_stack_top:

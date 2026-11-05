# OpenSBI在S-mode进入该入口：a0=hartid，a1=DTB地址。
# 本实验不使用DTB，因此忽略a1。

.section .text.entry
.global _entry
_entry:
        # 本实验的 NCPU 固定为 2，避免异常 hart 在设置栈前越界。
        li t3, 2
        bgeu a0, t3, spin
        # CPU_stack 定义于start.c中
        # sp = CPU_stack + ((hartid + 1) * 4096)
        # 将sp置于当前CPU的内核栈的栈顶
        la t0, CPU_stack
        li t1, 4096
        addi t2, a0, 1
        mul t1, t1, t2
        add sp, t0, t1
        # 跳转到start
        call start
spin:
        wfi
        j spin

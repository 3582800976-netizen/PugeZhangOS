#include "../arch/mod.h"

// 每个CPU在运行操作系统时需要一个初始的函数栈
__attribute__((aligned(16))) uint8 CPU_stack[4096 * NCPU];

extern void main();

uint64 boot_dtb;

void start(uint64 hartid, uint64 dtb)
{
    // OpenSBI已在S-mode进入内核。HSM启动hart时satp也为0。
    w_satp(0);
    sfence_vma();

    // 首次入口 a1 是 DTB；HSM 唤醒从核时 opaque 为 0。
    if (dtb) boot_dtb = dtb;
    w_tp(hartid);
    main();
    for (;;) asm volatile("wfi");
}

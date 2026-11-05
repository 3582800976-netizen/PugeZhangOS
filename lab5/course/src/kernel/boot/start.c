#include "../arch/mod.h"
#include "../trap/mod.h"

// 每个CPU在运行操作系统时需要一个初始的函数栈
__attribute__((aligned(16))) uint8 CPU_stack[4096 * NCPU];
uint64 boot_hartid;
uint64 boot_dtb;

extern void main();

void start(uint64 hartid, uint64 dtb)
{
    // OpenSBI已在S-mode进入内核。HSM启动hart时satp也为0。
    w_satp(0);
    sfence_vma();

    // 只有主入口的 a1 是设备树地址，HSM 次级入口的 opaque 为 0。
    if (dtb) boot_dtb = dtb;
    w_tp(hartid);
    main();
    for (;;) asm volatile("wfi");
}

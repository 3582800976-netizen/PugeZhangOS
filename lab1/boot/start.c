/* Lab 1：自己完成 M 模式到 S 模式的切换。 */
#include "platform.h"
#include "riscv.h"
#include "boottrace.h"

void kernel_main(uint64 hartid, int primary);

void boot_start(uint64 hartid, int primary)
{
    w_tp(hartid);
    boot_trace_mark(hartid, BOOT_ENTRY | BOOT_STACK);

    w_mie(0);
    w_mcounteren(7);                  /* 允许 S 模式读取时间和计数器 */
    w_pmpaddr0(~0UL);
    w_pmpcfg0(0x0f);                  /* 暂时允许 S 模式读写执行物理内存 */
    w_medeleg(0xffffUL & ~(1UL << 9)); /* 普通异常交给 S 模式诊断 */
    w_mideleg((1UL << 1) | (1UL << 5) | (1UL << 9));
    w_satp(0);                       /* Lab 1 直接使用物理地址 */
    w_sie(0);                        /* 本实验不启用设备和时钟中断 */
    w_mstatus((r_mstatus() & ~(MSTATUS_MPP_MASK | MSTATUS_MIE)) |
              MSTATUS_MPP_S);
    w_mepc((uint64)kernel_main);      /* mret 后从 kernel_main 继续 */

    /* mret 不是普通 C 函数调用，要显式放好目标函数的两个参数。 */
    register uint64 arg0 asm("a0") = hartid;
    register uint64 arg1 asm("a1") = (uint64)primary;
    asm volatile("mret" : : "r"(arg0), "r"(arg1) : "memory");
    __builtin_unreachable();
}

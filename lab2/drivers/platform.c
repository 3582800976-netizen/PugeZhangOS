/* QEMU virt 平台退出设备，不是 SBI 固件调用。
 * OpenSBI 0.7 的旧关机路径使用 16 位写入，但 QEMU 5.1 接受 32 位。
 * 页表为此设备保留 RW 映射，内核直接写入 32 位关机值。
 */
#include "platform.h"
#include "riscv.h"

void platform_shutdown(void)
{
    intr_off();
    fence_io();
    *(volatile uint32 *)QEMU_FINISHER = 0x5555;
    fence_io();
    for (;;) cpu_wait();
}

/* QEMU virt 的测试退出设备：写入成功标志后，QEMU 正常结束。 */
#include "types.h"
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

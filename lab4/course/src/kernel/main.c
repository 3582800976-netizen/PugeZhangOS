#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"
#include "trap/mod.h"
//_entry在kernel.ld中定义 将后续hart中的启动地址定义为该地址
extern void _entry(void);

static volatile int initialized = 0;

void main(void)
{
    uint64 hartid = r_tp();

    if (hartid == 0)
    {
        print_init();
        printf("cpu %d is booting with OpenSBI!\n", (int)hartid);
        pmem_init();
        kvm_init();

        // trap中各个CPU共享的初始化: PLIC和系统时钟
        // 必须放在其他hart启动之前, 否则它们可能在初始化完成前就走进中断
        trap_kernel_init();

        __sync_synchronize();
        initialized = 1;

        for (uint64 id = 1; id < NCPU; id++)
        {
            sbi_ret_t ret = sbi_hart_start(id, (uint64)_entry, 0);
            if (ret.error != SBI_SUCCESS)
            {
                printf("failed to start cpu %d: error %d\n",
                       (int)id, (int)ret.error);
                panic("sbi_hart_start");
            }
        }
    }
    else
    {
        while (!initialized)
            ;
        __sync_synchronize();
        printf("cpu %d is booting with OpenSBI!\n", (int)hartid);
    }

    // 以下初始化是每个CPU各自独有的

    // 切换到内核页表 (必须等CPU-0把内核页表建好)
    kvm_inithart();

    // PLIC开关, 中断入口stvec, 中断使能位
    trap_kernel_inithart();

    // 设置本CPU的第一次时钟事件, 并打开S-mode时钟中断
    timer_init();

    while (1)
        asm volatile("wfi");
}

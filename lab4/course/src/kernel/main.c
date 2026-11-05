#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"
#include "trap/mod.h"
#include "proc/method.h"
extern void _entry(void);

// OpenSBI 选择一个启动 hart；所有共享初始化在 HSM 启动其他核前完成。
static int boot_hart = -1;
static int initialized;

void main(void)
{
    intr_off();
    int hartid = (int)r_tp();
    int expected = -1;
    if (__atomic_compare_exchange_n(&boot_hart, &expected, hartid, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();
        trap_kernel_init();
        __atomic_store_n(&initialized, 1, __ATOMIC_RELEASE);
        for (int id = 0; id < NCPU; id++) {
            if (id == hartid) continue;
            sbi_ret_t ret = sbi_hart_start(id, (uint64)_entry, 0);
            if (ret.error != SBI_SUCCESS) {
                printf("failed to start cpu %d: error %d\n", id, (int)ret.error);
                panic("sbi_hart_start");
            }
        }
    } else {
        while (!__atomic_load_n(&initialized, __ATOMIC_ACQUIRE)) ;
        kvm_inithart();
    }
    // 先准备好中断入口、控制器和时钟，再打开 S 模式全局中断。
    trap_kernel_inithart();
    timer_init();
    intr_on();
    printf("cpu %d is ready with interrupts!\n", hartid);
    // 按课程契约，首进程固定在 hart 0 上运行。
    if (hartid == 0) {
        proc_make_first();
        intr_on();
        printf("[lab4] user process stopped; kernel remains alive\n");
    }
    while (1) asm volatile("wfi");
}

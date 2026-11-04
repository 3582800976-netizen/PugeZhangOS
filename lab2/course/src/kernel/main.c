#include "arch/mod.h"
#include "lib/mod.h"
#include "mem/mod.h"
extern void _entry(void);

// 固件选择一个初始 hart；HSM 从核启动前共享初始化已经完成。
static int boot_hart = -1;
static int initialized;

void main(void)
{
    int hartid = (int)r_tp();
    int expected = -1;
    if (__atomic_compare_exchange_n(&boot_hart, &expected, hartid, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();
        printf("cpu %d is booting with OpenSBI!\n", hartid);
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
        printf("cpu %d is booting with OpenSBI!\n", hartid);
    }
    while (1) asm volatile("wfi");
}

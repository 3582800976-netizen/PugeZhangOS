#include "arch/mod.h"
#include "lib/mod.h"

// CPU 0 初始化共享串口和打印锁，其他 CPU 等待初始化发布。
static int started;

int main()
{
    int cpuid = (int)r_tp();
    if (cpuid == 0) {
        print_init();
        __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
    } else {
        while (!__atomic_load_n(&started, __ATOMIC_ACQUIRE))
            ;
    }
    printf("cpu %d is booting!\n", cpuid);
    for (;;) asm volatile("wfi");
}

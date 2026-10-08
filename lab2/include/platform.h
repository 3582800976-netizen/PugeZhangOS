/* QEMU virt 硬件约定；本实验固定 128 MiB RAM。 */
#ifndef PUGE_PLATFORM_H
#define PUGE_PLATFORM_H
#ifndef CPUS
#define CPUS 3
#endif
#define MAX_HARTS 8
#define BOOT_STACK_SIZE 4096
#if CPUS < 1 || CPUS > MAX_HARTS
#error "CPUS must be in 1..8"
#endif
#define RAM_BASE       0x80000000UL
#define MEM_SIZE       (128UL * 1024 * 1024)
#define PHYS_END       (RAM_BASE + MEM_SIZE)
#define RAM_END        PHYS_END
/* OpenSBI 占用 RAM 前缀，内核从其后 2 MiB 开始。 */
#define KERNEL_BASE    0x80200000UL
#define UART0_BASE     0x10000000UL
/* PLIC 地址只进入页表；中断驱动在 Lab 3 实现。 */
#define PLIC_BASE      0x0c000000UL
#define PLIC_SIZE      0x04000000UL
#define TIMEBASE_HZ    10000000UL
#define QEMU_FINISHER 0x00100000UL

#ifndef __ASSEMBLER__
void platform_shutdown(void) __attribute__((noreturn));
#endif
#endif

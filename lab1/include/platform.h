/* Lab 1 使用的 QEMU virt 硬件约定。 */
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
#define KERNEL_BASE 0x80000000UL
#define UART0_BASE  0x10000000UL
#define TIMEBASE_HZ 10000000UL
#define QEMU_FINISHER 0x00100000UL

#ifndef __ASSEMBLER__
void platform_shutdown(void) __attribute__((noreturn));
#endif
#endif

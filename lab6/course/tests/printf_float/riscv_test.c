#include "src/kernel/lib/mod.h"
static void print_case(uint64 bits, int id)
{
    union { uint64 bits; double value; } number = { .bits = bits };
    printf("case%d=%f next=%d\n", id, number.value, 42);
}
void uart_init(void) {}
void uart_putc_sync(int c)
{
    volatile unsigned char *uart = (volatile unsigned char *)0x10000000UL;
    while (!(uart[5] & 0x20)) {}
    uart[0] = (unsigned char)c;
}
void spinlock_init(spinlock_t *lk, char *name) { (void)lk; (void)name; }
void spinlock_acquire(spinlock_t *lk) { (void)lk; }
void spinlock_release(spinlock_t *lk) { (void)lk; }
void memset(void *dst, uint8 value, uint32 n)
{
    uint8 *p = dst;
    for (uint32 i = 0; i < n; i++) p[i] = value;
}
extern int float_register_probe(void);
void main(void)
{
    print_case(0, 0);
    print_case(0x8000000000000000ULL, 1);
    print_case(0x3ff8000000000000ULL, 2);
    print_case(0xbff8000000000000ULL, 3);
    print_case(0x3fb999999999999aULL, 4);
    print_case(0x3eb0c6f7a0b5ed8dULL, 5);
    print_case(0x43efffffffffffffULL, 6);
    print_case(1, 7);
    printf("registers=%d\n", float_register_probe());
    printf("[float-test] done\n");
}

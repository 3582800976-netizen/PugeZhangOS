#include "src/kernel/lib/mod.h"
extern long read(int, void *, unsigned long);
extern long write(int, const void *, unsigned long);
static char out[128];
static unsigned count;
void uart_putc_sync(int c) { if (count < sizeof(out)) out[count++] = (char)c; }
void uart_init(void) {}
void spinlock_init(spinlock_t *lk, char *name) { (void)lk; (void)name; }
void spinlock_acquire(spinlock_t *lk) { (void)lk; }
void spinlock_release(spinlock_t *lk) { (void)lk; }
int main(void)
{
    uint64 value;
    for (;;) {
        unsigned received = 0;
        while (received < sizeof(value)) {
            long n = read(0, (char *)&value + received, sizeof(value) - received);
            if (n <= 0) return received == 0 ? 0 : 2;
            received += n;
        }
        count = 0;
        union { uint64 bits; double number; } item = { .bits = value };
        printf("%f", item.number);
        uart_putc_sync('\n');
        if (write(1, out, count) != (long)count) return 3;
    }
}

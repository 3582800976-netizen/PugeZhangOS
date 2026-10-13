// One formatter, two destinations: UART output and bounded self-test buffers.
#include <stdarg.h>
#include "console.h"
#include "uart.h"
#include "memory.h"
#include "spinlock.h"

static struct spinlock output_lock;
struct output { char *buffer; uint64 capacity; int count; int serial; };

static void emit(struct output *out, char c)
{
    if (out->serial) uart_putc(c);
    else if (out->capacity && (uint64)out->count < out->capacity - 1)
        out->buffer[out->count] = c;
    out->count++;
}

static void emit_unsigned(struct output *out, uint64 value, unsigned base)
{
    char digits[20];
    unsigned used = 0;
    do {
        unsigned digit = value % base;
        digits[used++] = digit < 10 ? '0' + digit : 'a' + digit - 10;
        value /= base;
    } while (value);
    while (used) emit(out, digits[--used]);
}

static void emit_signed(struct output *out, int64 value)
{
    uint64 magnitude = (uint64)value;
    if (value < 0) {
        emit(out, '-');
        // Unsigned subtraction also handles LONG_MIN without signed overflow.
        magnitude = 0UL - magnitude;
    }
    emit_unsigned(out, magnitude, 10);
}

static int format(struct output *out, const char *fmt, va_list ap)
{
    while (*fmt) {
        if (*fmt != '%') { emit(out, *fmt++); continue; }
        fmt++;
        if (!*fmt) { emit(out, '%'); break; }
        int wide = 0;
        if (*fmt == 'l') {
            wide = 1;
            fmt++;
            if (!*fmt) { emit(out, '%'); emit(out, 'l'); break; }
        }
        char spec = *fmt++;
        switch (spec) {
        case 'd': emit_signed(out, wide ? va_arg(ap, long) : va_arg(ap, int)); break;
        case 'u': emit_unsigned(out, wide ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int), 10); break;
        case 'x': emit_unsigned(out, wide ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int), 16); break;
        case 'p':
            emit(out, '0'); emit(out, 'x');
            emit_unsigned(out, (uint64)va_arg(ap, void *), 16);
            break;
        case 'c': emit(out, (char)va_arg(ap, int)); break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s) emit(out, *s++);
            break;
        }
        case '%': emit(out, '%'); break;
        default:
            emit(out, '%');
            if (wide) emit(out, 'l');
            emit(out, spec);
            break;
        }
    }
    if (!out->serial && out->capacity) {
        uint64 end = (uint64)out->count < out->capacity ? (uint64)out->count : out->capacity - 1;
        out->buffer[end] = '\0';
    }
    return out->count;
}

void console_init(void)
{
    spin_init(&output_lock);
    uart_init();
}

void console_putc(char c)
{
    spin_acquire(&output_lock);
    uart_putc(c);
    spin_release(&output_lock);
}

int console_printf(const char *fmt, ...)
{
    struct output out = {0, 0, 0, 1};
    va_list ap;
    va_start(ap, fmt);
    spin_acquire(&output_lock);
    int count = format(&out, fmt, ap);
    spin_release(&output_lock);
    va_end(ap);
    return count;
}

int console_snprintf(char *dst, uint64 capacity, const char *fmt, ...)
{
    struct output out = {dst, capacity, 0, 0};
    va_list ap;
    va_start(ap, fmt);
    int count = format(&out, fmt, ap);
    va_end(ap);
    return count;
}

int console_selftest(void)
{
    char buffer[160];
    const char *expected = "-2147483648 0 4294967295 ffffffff 0x80200000 A ok %";
    int n = console_snprintf(buffer, sizeof(buffer), "%d %u %u %x %p %c %s %%",
                             (-2147483647 - 1), 0u, 4294967295u, 0xffffffffu,
                             (void *)0x80200000UL, 'A', "ok");
    int failures = strcmp(buffer, expected) != 0 || n != (int)strlen(expected);
    console_snprintf(buffer, sizeof(buffer), "tail%");
    failures += strcmp(buffer, "tail%") != 0;
    console_snprintf(buffer, sizeof(buffer), "%ld %lu", (-9223372036854775807L - 1), ~0UL);
    failures += strcmp(buffer, "-9223372036854775808 18446744073709551615") != 0;
    console_snprintf(buffer, sizeof(buffer), "%s %q", (char *)0);
    failures += strcmp(buffer, "(null) %q") != 0;
    char tiny[4];
    n = console_snprintf(tiny, sizeof(tiny), "abcdef");
    failures += n != 6 || strcmp(tiny, "abc") != 0;
    failures += console_snprintf(0, 0, "abcdef") != 6;
    console_printf("[test] format %s\n", failures ? "FAIL" : "PASS");
    return failures;
}

void panic(const char *message)
{
    asm volatile("csrci sstatus, 2" : : : "memory");
    // Panic must work even if failure occurred while the output lock was held.
    uart_puts("\n[PANIC] ");
    uart_puts(message);
    uart_putc('\n');
    for (;;) asm volatile("wfi");
}

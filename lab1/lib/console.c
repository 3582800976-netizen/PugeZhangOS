/* 一份格式化逻辑，对应两个输出目标：真实串口、有限长度的测试缓冲区。 */
#include <stdarg.h>
#include "console.h"
#include "uart.h"
#include "spinlock.h"

static struct spinlock output_lock;
/* count 记录完整输出长度，即使缓冲区放不下，也继续计数。 */
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
    /* 先从个位开始拆数字，再倒序输出；do/while 也能打印数字 0。 */
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
        /* 无符号减法也能处理最小负数，不会触发有符号整数溢出。 */
        magnitude = 0UL - magnitude;
    }
    emit_unsigned(out, magnitude, 10);
}

static int format(struct output *out, const char *fmt, va_list ap)
{
    /* va_list 按格式串读取不定数量参数，例如 %d 对应一个整数。 */
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


void panic(const char *message)
{
    asm volatile("csrci sstatus, 2" : : : "memory");
    /* 错误可能发生在持有输出锁时，因此停机信息绕过锁直接写串口。 */
    uart_puts("\n[PANIC] ");
    uart_puts(message);
    uart_putc('\n');
    for (;;) asm volatile("wfi");
}

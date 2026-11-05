/* 标准输出和报错机制 */

#include "mod.h"

static char digits[] = "0123456789abcdef";

/* printf的自旋锁 */
static spinlock_t print_lk;

/* 初始化uart + 初始化printf锁 */
void print_init(void)
{
    uart_init();
    spinlock_init(&print_lk, "printf");
}

/* %d %p */
static void printint(int xx, int base, int sign)
{
    char buf[16];
    int i;
    uint32 x;

    if (sign && (sign = xx < 0))
        x = 0U - (uint32)xx; // 避免 INT_MIN 取负的有符号溢出
    else
        x = xx;

    i = 0;
    do
    {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i++] = '-';

    while (--i >= 0)
        uart_putc_sync(buf[i]);
}

/* %x */
static void printptr(uint64 x)
{
    uart_putc_sync('0');
    uart_putc_sync('x');
    for (unsigned i = 0; i < (sizeof(uint64) * 2); i++, x <<= 4)
        uart_putc_sync(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

/* 整数运算格式化 IEEE-754 binary64，避免内核改动用户浮点寄存器。 */
static void printfloat_bits(uint64 bits, unsigned precision)
{
    uint64 mantissa = bits & ((1ULL << 52) - 1);
    unsigned exponent = (unsigned)((bits >> 52) & 0x7ff);
    if (bits >> 63)
        uart_putc_sync('-');
    if (exponent == 0x7ff) {
        const char *s = mantissa ? "nan" : "inf";
        while (*s) uart_putc_sync(*s++);
        return;
    }
    if (exponent >= 1023 + 64) {
        const char *s = "overflow";
        while (*s) uart_putc_sync(*s++);
        return;
    }
    int shift = (exponent ? (int)exponent - 1023 : -1022) - 52;
    if (exponent)
        mantissa |= 1ULL << 52;
    uint64 integer = 0;
    // binary64 的最小次正规数分母为 2^1074；17 个 64 位字足够。
    uint64 remainder[17] = {0};
    unsigned fractional_bits = 0;
    if (shift >= 0) {
        integer = mantissa << shift;
    } else {
        fractional_bits = (unsigned)-shift;
        if (fractional_bits < 64) {
            integer = mantissa >> fractional_bits;
            remainder[0] = mantissa & ((1ULL << fractional_bits) - 1);
        } else {
            remainder[0] = mantissa;
        }
    }
    char buf[20];
    unsigned used = 0;
    do {
        buf[used++] = '0' + integer % 10;
        integer /= 10;
    } while (integer);
    while (used)
        uart_putc_sync(buf[--used]);
    uart_putc_sync('.');
    for (unsigned i = 0; i < precision; i++) {
        unsigned digit = 0;
        if (fractional_bits) {
            // 对多字余数乘十，逐位输出，避免浮点和 128 位运行时。
            uint64 carry = 0;
            for (unsigned j = 0; j < 17; j++) {
                uint64 old = remainder[j];
                uint64 twice = old << 1, eight = old << 3;
                uint64 product = twice + eight;
                uint64 next_carry = (old >> 63) + (old >> 61) + (product < twice);
                uint64 sum = product + carry;
                next_carry += sum < product;
                remainder[j] = sum;
                carry = next_carry;
            }
            unsigned word = fractional_bits / 64;
            unsigned bit = fractional_bits % 64;
            if (bit == 0) {
                digit = (unsigned)remainder[word];
                remainder[word] = 0;
            } else {
                digit = (unsigned)(remainder[word] >> bit);
                if (word + 1 < 17)
                    digit |= (unsigned)(remainder[word + 1] << (64 - bit));
                remainder[word] &= (1ULL << bit) - 1;
            }
            for (unsigned j = word + 1; j < 17; j++)
                remainder[j] = 0;
        }
        uart_putc_sync('0' + digit);
    }
}

/*
    标准化输出, 需要支持:
    1. %d (32位有符号数,以10进制输出)
    2. %p (32位无符号数,以16进制输出)
    3. %x (64位无符号数,以0x开头的16进制输出)
    4. %c (单个字符)
    5. %s (字符串)
    提示: stdarg.h中的va_list中包括你需要的参数地址
*/
void printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    spinlock_acquire(&print_lk);
    while (*fmt) {
        if (*fmt != '%') {
            uart_putc_sync(*fmt++);
            continue;
        }
        fmt++;
        if (!*fmt)
            break;
        switch (*fmt++) {
        case 'd': printint(va_arg(ap, int), 10, 1); break;
        case 'p': printint((int)va_arg(ap, uint32), 16, 0); break;
        case 'x': printptr(va_arg(ap, uint64)); break;
        case 'c': uart_putc_sync(va_arg(ap, int)); break;
        case 'f': {
            double value = va_arg(ap, double);
            uint64 bits;
            __builtin_memcpy(&bits, &value, sizeof(bits));
            printfloat_bits(bits, 6);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s) uart_putc_sync(*s++);
            break;
        }
        case '%': uart_putc_sync('%'); break;
        default: uart_putc_sync('%'); uart_putc_sync(fmt[-1]); break;
        }
    }
    spinlock_release(&print_lk);
    va_end(ap);
}



/* 如果发生panic, UART的停止标志 */
volatile int panicked = 0;

// panic 不走锁及其依赖的中断深度检查，避免在锁错误时递归 panic。
static void panic_putc(int c)
{
    while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
        ;
    WriteReg(THR, c);
}

/* 报错并终止输出 */
void panic(const char *s)
{
    intr_off();
    const char *prefix = "panic! ";
    while (*prefix) panic_putc(*prefix++);
    if (s) while (*s) panic_putc(*s++);
    panic_putc('\n');
    panicked = 1;
    while (1)
        ;
}

/* 如果不满足条件, 则调用panic */
void assert(bool condition, const char *warning)
{
    if (!condition) panic(warning);
}

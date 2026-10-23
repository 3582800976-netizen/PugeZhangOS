/* User code has no UART, kernel printf, or supervisor-register access. */
#include "types.h"
#include "user_layout.h"
#include "syscall.h"
#include "user_api.h"

static volatile uint64 initialized = 0x1122334455667788UL;
static volatile uint64 sum;
extern int user_register_probe(long expected_pid);

#define call user_syscall3

static int zero_pages(uint64 address, uint64 pages)
{
    volatile uint8 *p = (volatile uint8 *)address;
    for (uint64 n = 0; n < pages * 4096; n++)
        if (p[n]) return 0;
    return 1;
}

static int heap_demo(void)
{
    struct user_heap_info info;
    if (user_heap_info(&info) || info.free_pages != USER_HEAP_PAGES) return 101;
    if (user_heap_dump(0)) return 102;
    long a = user_alloc_pages(2, 0), b = user_alloc_pages(3, 0), c = user_alloc_pages(2, 0);
    if (a != USER_HEAP_BASE || b != a + 8192 || c != b + 12288) return 103;
    if (!zero_pages(a, 7)) return 104;
    volatile uint8 *bytes = (volatile uint8 *)a;
    for (uint64 n = 0; n < 7 * 4096; n++) bytes[n] = (uint8)(n * 17 + 3);
    for (uint64 n = 0; n < 7 * 4096; n++)
        if (bytes[n] != (uint8)(n * 17 + 3)) return 105;
    /* Both kernel-to-user INFO and user-to-kernel WRITE genuinely cross a page. */
    struct user_heap_info *cross = (struct user_heap_info *)(a + 4096 - 16);
    if (user_heap_info(cross) || cross->allocated_pages != 7) return 106;
    static const char message[] = "heap-ok\n";
    for (unsigned n = 0; n < sizeof message - 1; n++) bytes[4092 + n] = message[n];
    if (call(SYS_WRITE, a + 4092, sizeof message - 1, 0) != (long)sizeof message - 1) return 107;
    if (user_heap_dump(1)) return 108;

    if (user_alloc_pages(0, 0) != SYS_EINVAL || user_alloc_pages(~0UL, 0) != SYS_EINVAL ||
        user_alloc_pages(1, a) != SYS_ENOMEM || user_alloc_pages(1, a + 1) != SYS_EINVAL ||
        user_alloc_pages(1, USER_CODE_BASE) != SYS_EINVAL ||
        user_free_pages(USER_CODE_BASE, 1) != SYS_EINVAL ||
        user_free_pages(a + 1, 1) != SYS_EINVAL || user_free_pages(a, 0) != SYS_EINVAL)
        return 109;
    if (call(SYS_HEAP_INFO, USER_CODE_BASE, sizeof info, 0) != SYS_EFAULT ||
        call(SYS_HEAP_INFO, (uint64)&info, sizeof info - 1, 0) != SYS_EINVAL ||
        call(SYS_HEAP_INFO, ~0UL - 7, sizeof info, 0) != SYS_EFAULT) return 110;
    volatile uint8 *tail = (volatile uint8 *)(c + 8192 - 32);
    for (unsigned n = 0; n < 32; n++) tail[n] = 0x5a;
    if (call(SYS_HEAP_INFO, (uint64)tail, sizeof info, 0) != SYS_EFAULT) return 111;
    for (unsigned n = 0; n < 32; n++) if (tail[n] != 0x5a) return 112;

    if (user_free_pages(b, 3) || user_heap_dump(2)) return 113;
    if (user_free_pages(b, 3) != SYS_EINVAL || user_free_pages(a, 3) != SYS_EINVAL) return 114;
    long d = user_alloc_pages(2, 0);
    if (d != b || !zero_pages(d, 2) || user_heap_dump(3)) return 115;
    if (user_free_pages(d, 2) || user_free_pages(a, 2) || user_free_pages(c, 2) ||
        user_heap_dump(4)) return 116;
    long exact = user_alloc_pages(2, USER_HEAP_BASE + 20 * 4096);
    if (exact != USER_HEAP_BASE + 20 * 4096 || user_heap_dump(5)) return 117;
    if (user_free_pages(exact, 2) || user_heap_dump(6) || user_heap_info(&info) ||
        info.free_pages != USER_HEAP_PAGES || info.regions != 1) return 118;
    /* Deliberately leave an allocation live: process exit must own its cleanup. */
    if (user_alloc_pages(1, 0) < 0) return 119;
    return call(SYS_REPORT, 0x5005, 0, 0) ? 120 : 0;
}

static int heap_fault(unsigned mode)
{
    long address = user_alloc_pages(2, 0);
    if (address != USER_HEAP_BASE) return 130;
    if (mode == 11) {
        *(volatile uint32 *)address = 0x00008067; /* ret, if the page were executable */
        ((void (*)(void))(uint64)address)();
    } else {
        if (user_free_pages(address, 1)) return 131;
        if (mode == 9) return *(volatile uint8 *)address;
        *(volatile uint8 *)address = 1;
    }
    return 132;
}

static uint64 user_tp(void)
{
    uint64 value;
    asm volatile("mv %0, tp" : "=r"(value));
    return value;
}

static int bad_calls(void)
{
    if (call(999, 0, 0, 0) != SYS_ENOSYS) return 21;
    if (call(SYS_WRITE, 0, 1, 0) != SYS_EFAULT) return 22;
    if (call(SYS_WRITE, ~0UL - 7, 16, 0) != SYS_EFAULT) return 23;
    if (call(SYS_WRITE, USER_DATA_BASE + 4096 - 8, 16, 0) != SYS_EFAULT) return 24;
    if (call(SYS_WRITE, TRAPFRAME, 8, 0) != SYS_EFAULT) return 25;
    if (call(SYS_WRITE, TRAMPOLINE, 8, 0) != SYS_EFAULT) return 26;
    if (call(SYS_WRITE, 0x80200000UL, 8, 0) != SYS_EFAULT) return 27;
    if (call(SYS_WRITE, USER_CODE_BASE, SYS_WRITE_MAX + 1, 0) != SYS_EINVAL) return 28;
    return user_tp() == 0xfeed ? 0 : 29;
}

long first_main(uint64 mode)
{
    if (initialized != 0x1122334455667788UL || sum != 0) return 10;
    if (user_tp() != 0xfeed) return 11;
    switch (mode) {
    case 1: (void)*(volatile uint8 *)0x80200000UL; return 201;
    case 2: (void)*(volatile uint8 *)0x10000000UL; return 202;
    case 3: *(volatile uint8 *)USER_CODE_BASE = 0; return 203;
    case 4: ((void (*)(void))(uint64)USER_STACK_BASE)(); return 204;
    case 5: (void)*(volatile uint8 *)(uint64)TRAPFRAME; return 205;
    case 6: (void)*(volatile uint8 *)(uint64)TRAMPOLINE; return 206;
    case 7: return bad_calls();
    case 8: return heap_demo();
    case 9: case 10: case 11: return heap_fault((unsigned)mode);
    case 0: break;
    default: return 207;
    }

    long pid = call(SYS_GETPID, 0, 0, 0);
    if (pid <= 0 || !user_register_probe(pid)) return 12;
    static const char message[] = "hello from U-mode\n";
    if (call(SYS_WRITE, (uint64)message, sizeof(message) - 1, 0) != (long)sizeof(message) - 1)
        return 13;
    int bad = bad_calls();
    if (bad) return bad;
    for (uint64 i = 1; i <= 100; i++) sum += i;
    if (sum != 5050) return 14;
    /* This count includes only timer interrupts delivered while we are in U.
     * Poll through real ecalls; kernel checks also inspect the recorded count. */
    /* Leave a visible 0.3-second U execution window for concurrent UART input. */
    while (call(SYS_TICKS, 0, 0, 0) < 30) {
        /* Spend time executing U instructions rather than immediately
         * re-entering S for another call; timer tests must exercise U traps. */
        for (unsigned delay = 0; delay < 32768; delay++) asm volatile("nop");
        if (user_tp() != 0xfeed) return 15;
    }
    if (user_tp() != 0xfeed) return 16;
    if (call(SYS_REPORT, sum, 0, 0) != 0) return 17;
    return 0;                    /* start.S sends this status through EXIT */
}

/* User code has no UART, kernel printf, or supervisor-register access. */
#include "types.h"
#include "user_layout.h"
#include "syscall.h"

static volatile uint64 initialized = 0x1122334455667788UL;
static volatile uint64 sum;
extern int user_register_probe(long expected_pid);

static long call(long number, uint64 x, uint64 y, uint64 z)
{
    register long a0 asm("a0") = (long)x;
    register uint64 a1 asm("a1") = y;
    register uint64 a2 asm("a2") = z;
    register long a7 asm("a7") = number;
    asm volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
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

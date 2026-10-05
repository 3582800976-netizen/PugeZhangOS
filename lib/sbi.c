#include "platform.h"
#include "sbi.h"
#include "riscv.h"
struct sbi_result { long error; long value; };
static struct sbi_result call_sbi(uint64 extension, uint64 function,
                                uint64 x, uint64 y, uint64 z)
{
    register uint64 a0 asm("a0") = x;
    register uint64 a1 asm("a1") = y;
    register uint64 a2 asm("a2") = z;
    register uint64 a6 asm("a6") = function;
    register uint64 a7 asm("a7") = extension;
    asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a6), "r"(a7)
                 : "memory");
    return (struct sbi_result){ (long)a0, (long)a1 };
}
int sbi_probe_extension(uint64 extension)
{
#if LAB == 1
    (void)extension;
    return 0;
#else
    struct sbi_result r = call_sbi(0x10, 3, extension, 0, 0);
    return r.error == 0 && r.value != 0;
#endif
}
long sbi_start_hart(uint64 hartid, uint64 opaque)
{
    extern void _boot(void);
    return call_sbi(SBI_EXT_HSM, 0, hartid, (uint64)_boot, opaque).error;
}
int sbi_boot_secondaries(uint64 primary, uint64 dtb, uint64 active_harts)
{
    if (!sbi_probe_extension(SBI_EXT_HSM)) return 0;
    int count = 0;
    for (uint64 hart = 0; hart < active_harts; ++hart) {
        if (hart == primary) continue;
        long error = sbi_start_hart(hart, dtb);
        if (error == 0 || error == -6) ++count;
        else return -1;
    }
    return count;
}
void sbi_set_timer(uint64 deadline)
{
    /* TIME on OpenSBI 0.7+; retain legacy EID 0 fallback. */
    if (sbi_probe_extension(SBI_EXT_TIME))
        (void)call_sbi(SBI_EXT_TIME, 0, deadline, 0, 0);
    else
        (void)call_sbi(0, 0, deadline, 0, 0);
}
void sbi_shutdown(void)
{
    /* Platform-specific compatibility exit for this QEMU machine.
     * Bundled OpenSBI 0.7's legacy shutdown uses a 16-bit finisher store;
     * QEMU 5.1 accepts only 32-bit stores and traps that firmware access.
     * Lab2/3 VM maps this test-finisher page separately as R/W.
     */
    intr_off();
    fence_io();
    *(volatile uint32 *)0x100000UL = 0x5555;
    fence_io();
    for (;;) cpu_wait();
}

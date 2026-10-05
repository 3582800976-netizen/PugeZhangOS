/* No printing in normal interrupts: record facts, leave work to main loop. */
#include "platform.h"
#include "riscv.h"
#include "trap.h"
#include "sbi.h"
#include "plic.h"
#include "console.h"
#include "uart.h"
void panic(const char *message) __attribute__((noreturn));
void uart_intr(void);
extern void kernel_vector(void);
extern char trap_breakpoint_site[];
extern int trap_register_probe(void);
static uint64 ticks[MAX_HARTS];
static uint64 uart_interrupts;
static uint64 breakpoint_count[MAX_HARTS];
static int breakpoint_expected[MAX_HARTS];
static uint64 expected_fault_cause[MAX_HARTS];
static uint64 expected_fault_address[MAX_HARTS];
static int fault_seen[MAX_HARTS];
extern char trap_load_fault_site[], trap_load_fault_resume[];
extern char trap_store_fault_site[], trap_store_fault_resume[];
extern int trap_load_probe(uint64 address);
extern int trap_store_probe(uint64 address);

static int test_access_fault(uint64 address, int store)
{
    uint64 hart = r_tp();
    if (hart >= MAX_HARTS) return 0;
    int was_enabled = intr_get();
    intr_off();
    expected_fault_address[hart] = address;
    fault_seen[hart] = 0;
    expected_fault_cause[hart] = store ? 15 : 13;
    asm volatile("" : : : "memory");
    int resumed = store ? trap_store_probe(address) : trap_load_probe(address);
    asm volatile("" : : : "memory");
    expected_fault_cause[hart] = 0;
    int ok = resumed && fault_seen[hart];
    if (was_enabled) intr_on();
    return ok;
}
int trap_test_load_fault(uint64 address) { return test_access_fault(address, 0); }
int trap_test_store_fault(uint64 address) { return test_access_fault(address, 1); }


void trap_init_hart(void)
{
    intr_off();
    w_sie(0);
    w_stvec((uint64)kernel_vector); /* aligned address means direct mode */
    w_sscratch(0);                /* no U-mode stack switching in Labs1-3 */
#if LAB == 3
    plic_init_hart();
    sbi_set_timer(r_time() + TIMER_INTERVAL);
#endif
}
void trap_enable(void)
{
#if LAB == 3
    w_sie(SIE_STIE | SIE_SEIE);
    intr_on();
#endif
}
uint64 trap_ticks(uint64 hartid)
{
    if (hartid >= MAX_HARTS) return 0;
    return __atomic_load_n(&ticks[hartid], __ATOMIC_RELAXED);
}
uint64 trap_uart_interrupts(void)
{
    return __atomic_load_n(&uart_interrupts, __ATOMIC_RELAXED);
}
int trap_breakpoint_test(void)
{
    uint64 hart = r_tp();
    if (hart >= MAX_HARTS) return 0;
    uint64 before = breakpoint_count[hart];
    breakpoint_expected[hart] = 1;
    asm volatile("" : : : "memory");
    int registers_ok = trap_register_probe();
    asm volatile("" : : : "memory");
    return registers_ok && !breakpoint_expected[hart] &&
           breakpoint_count[hart] == before + 1;
}
uint64 trap_handle(uint64 cause, uint64 pc, uint64 value)
{
    uint64 hart = r_tp();
    if (hart >= MAX_HARTS) panic("trap: invalid hart ID");
#if LAB == 3
    if (cause == ((1UL << 63) | 5)) {
        __atomic_fetch_add(&ticks[hart], 1, __ATOMIC_RELAXED);
        sbi_set_timer(r_time() + TIMER_INTERVAL);
        return pc;                /* no guessed PC+4: sepc is continuation */
    }
    if (cause == ((1UL << 63) | 9)) {
        uint32 irq;
        while ((irq = plic_claim()) != 0) {
            if (irq == UART_IRQ) {
                __atomic_fetch_add(&uart_interrupts, 1, __ATOMIC_RELAXED);
                uart_intr();
            }
            plic_complete(irq);   /* finish even an unexpected device source */
        }
        return pc;
    }
#endif
    if (expected_fault_cause[hart] == cause &&
        expected_fault_address[hart] == value) {
        uint64 site = cause == 13 ? (uint64)trap_load_fault_site :
                                   (uint64)trap_store_fault_site;
        if ((cause == 13 || cause == 15) && pc == site) {
            fault_seen[hart] = 1;
            expected_fault_cause[hart] = 0;
            return cause == 13 ? (uint64)trap_load_fault_resume :
                                 (uint64)trap_store_fault_resume;
        }
    }
    if (cause == 3 && breakpoint_expected[hart] &&
        pc == (uint64)trap_breakpoint_site) {
        breakpoint_expected[hart] = 0;
        ++breakpoint_count[hart];
        return pc + 4;            /* this deliberately emitted ebreak is 4B */
    }
    /* A fault can occur while the console lock is held: diagnose without it. */
    uart_puts("trap: scause=");
    for (int shift = 60; shift >= 0; shift -= 4)
        uart_putc("0123456789abcdef"[(cause >> shift) & 15]);
    uart_puts(" sepc=");
    for (int shift = 60; shift >= 0; shift -= 4)
        uart_putc("0123456789abcdef"[(pc >> shift) & 15]);
    uart_puts(" stval=");
    for (int shift = 60; shift >= 0; shift -= 4)
        uart_putc("0123456789abcdef"[(value >> shift) & 15]);
    uart_puts("\n");
    panic("unexpected supervisor trap");
}

int trap_selftest(void) { return trap_breakpoint_test() ? 0 : 1; }

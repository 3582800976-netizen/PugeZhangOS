/* Minimal synchronous exception support for Lab 2's page-permission tests.
 * A page fault is caused by the instruction currently executing. It still
 * occurs with SIE disabled. Timer/device interrupts are added only in Lab 3.
 */
#include "platform.h"
#include "riscv.h"
#include "trap.h"
#include "console.h"
#include "uart.h"

extern void kernel_vector(void);
extern char trap_load_fault_site[], trap_load_fault_resume[];
extern char trap_store_fault_site[], trap_store_fault_resume[];
extern int trap_load_probe(uint64 address);
extern int trap_store_probe(uint64 address);

static uint64 expected_fault_cause[MAX_HARTS];
static uint64 expected_fault_address[MAX_HARTS];
static int fault_seen[MAX_HARTS];

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
    w_sie(0);                     /* No hardware interrupts in Lab 2. */
    w_stvec((uint64)kernel_vector);
    w_sscratch(0);                /* All code runs on the current kernel stack. */
}

uint64 trap_handle(uint64 cause, uint64 pc, uint64 value)
{
    uint64 hart = r_tp();
    if (hart >= MAX_HARTS) panic("trap: invalid hart ID");
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
    /* Unexpected faults remain fatal, even when a console lock was held. */
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

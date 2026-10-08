/* Display snapshots without changing the mechanisms being observed. */
#include "monitor.h"
#include "bootstate.h"
#include "console.h"
#include "platform.h"
#include "page.h"
#include "vm.h"
#include "trap.h"
#include "uart.h"

extern char _text_start[], _rodata_start[], _data_start[];

void diagnostics_boot(void)
{
    for (unsigned id = 0; id < CPUS; id++) {
        struct boot_snapshot s;
        boot_snapshot(id, &s);
        console_printf("[boot] hart=%u stages=0x%x ready=%u stack=0x%lx..0x%lx satp=0x%lx\n",
                       id, s.stages, s.ready, s.stack_low, s.stack_high, s.satp);
    }
}

void diagnostics_memory(void)
{
    for (int pool = PAGE_KERNEL; pool <= PAGE_USER; pool++) {
        struct page_stats s;
        page_get_stats(pool, &s);
        console_printf("[mem] pool=%s total=%lu free=%lu allocated=%lu rejected=%lu range=0x%lx..0x%lx\n",
                       pool == PAGE_KERNEL ? "kernel" : "user", s.total, s.free, s.total - s.free,
                       s.rejected, s.base, s.limit);
    }
}

static void show_mapping(const char *name, uint64 address)
{
    struct vm_mapping m;
    int result = vm_lookup(address, &m);
    if (result != VM_OK) {
        console_printf("[vm] %s va=0x%lx %s\n", name, address,
                       result == VM_BAD_ADDRESS ? "noncanonical" : "unmapped");
        return;
    }
    char perms[4];
    int n = 0;
    if (m.flags & VM_R) perms[n++] = 'R';
    if (m.flags & VM_W) perms[n++] = 'W';
    if (m.flags & VM_X) perms[n++] = 'X';
    perms[n] = '\0';
    console_printf("[vm] %s va=0x%lx pa=0x%lx perms=%s vpn=%u/%u/%u level=%u pte=0x%lx\n",
                   name, address, m.pa, perms, m.vpn[2], m.vpn[1], m.vpn[0], m.level, m.pte);
}

static int parse_hex(const char *s, uint64 *value)
{
    uint64 v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (!*s) return -1;
    for (; *s; s++) {
        unsigned d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else return -1;
        if (v > (~0UL - d) / 16) return -1;
        v = v * 16 + d;
    }
    *value = v;
    return 0;
}

void diagnostics_vm(const char *argument)
{
    if (*argument) {
        uint64 address;
        if (parse_hex(argument, &address))
            console_printf("[vm] invalid hexadecimal address\n");
        else show_mapping("QUERY", address);
    } else {
        show_mapping("UART", UART0_BASE);
        show_mapping("PLIC", PLIC_BASE);
        show_mapping("TEXT", (uint64)_text_start);
        show_mapping("RODATA", (uint64)_rodata_start);
        show_mapping("DATA", (uint64)_data_start);
        show_mapping("ALIAS", VM_DIAG_ALIAS);
        show_mapping("FIRMWARE", 0x80000000UL);
        show_mapping("CLINT", 0x02000000UL);
    }
}

void diagnostics_ticks(void)
{
    for (unsigned id = 0; id < CPUS; id++)
        console_printf("[ticks] hart=%u count=%lu\n", id, trap_ticks(id));
}

void diagnostics_irq(void)
{
    console_printf("[irq] uart_rx=%lu bytes=%lu dropped=%lu\n",
                   trap_uart_interrupts(), uart_rx_bytes(), uart_rx_dropped());
}

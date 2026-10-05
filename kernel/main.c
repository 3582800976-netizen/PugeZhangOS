// A small kernel monitor observes the mechanisms built in Labs 1--3.
// It executes in supervisor mode; no user processes or user shell yet.
#include "types.h"
#include "platform.h"
#include "riscv.h"
#include "console.h"
#include "uart.h"
#include "memory.h"
#include "spinlock.h"
#include "boottrace.h"
#include "sbi.h"
#include "trap.h"
#include "plic.h"
#if LAB >= 2
#include "page.h"
#include "vm.h"
#endif

extern char _boot_stack_top[], _text_start[], _rodata_start[], _data_start[];
struct boot_record { uint32 stages, ready; uint64 satp; };
static struct boot_record records[MAX_HARTS];
static uint32 global_ready, ready_count;
static uint64 leader;
static int boot_failures;
#if LAB >= 2
static void *held_kernel[MAX_HARTS], *held_user[MAX_HARTS];
static uint32 held_count, allow_release;
static int hart_page_failures[MAX_HARTS];
static uint64 baseline_free[2];
#endif

static uint64 read_satp(void)
{
    uint64 value;
    asm volatile("csrr %0, satp" : "=r"(value));
    return value;
}

static uint64 read_time(void)
{
    uint64 value;
    asm volatile("rdtime %0" : "=r"(value));
    return value;
}

void boot_trace_mark(uint64 id, uint32 event)
{
    records[id].stages |= event;
}

static void wait_count(uint32 *count, uint32 target)
{
    uint64 deadline = read_time() + 5UL * TIMEBASE_HZ;
    while (__atomic_load_n(count, __ATOMIC_ACQUIRE) != target) {
        if (read_time() > deadline) panic("hart rendezvous timed out; CPUS must match QEMU -smp");
        asm volatile("nop");
    }
}

static void show_boot(void)
{
    for (unsigned id = 0; id < CPUS; id++) {
        uint64 top = (uint64)_boot_stack_top - (uint64)id * BOOT_STACK_SIZE;
        console_printf("[boot] hart=%u stages=0x%x ready=%u stack=0x%lx..0x%lx satp=0x%lx\n",
                       id, records[id].stages, __atomic_load_n(&records[id].ready, __ATOMIC_ACQUIRE),
                       top - BOOT_STACK_SIZE, top, records[id].satp);
    }
}

#if LAB >= 2
static uint32 read_be32(const uint8 *p)
{
    return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | p[3];
}

static uint64 dtb_length(uint64 address)
{
    if (address < KERNEL_BASE || address > PHYS_END - 8) return 0;
    const uint8 *p = (const uint8 *)address;
    if (read_be32(p) != 0xd00dfeedU) return 0;
    uint64 size = read_be32(p + 4);
    return size >= 40 && size <= PHYS_END - address ? size : 0;
}

static void show_memory(void)
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

static void parallel_pages(uint64 id, int primary)
{
    int failed = 0;
    held_kernel[id] = page_alloc(PAGE_KERNEL);
    held_user[id] = page_alloc(PAGE_USER);
    if (!held_kernel[id] || !held_user[id]) failed++;
    __atomic_fetch_add(&held_count, 1, __ATOMIC_ACQ_REL);
    if (primary) {
        wait_count(&held_count, CPUS);
        for (unsigned i = 0; i < CPUS; i++)
            for (unsigned j = 0; j < i; j++)
                if (held_kernel[i] == held_kernel[j] || held_user[i] == held_user[j]) failed++;
        __atomic_store_n(&allow_release, 1, __ATOMIC_RELEASE);
    } else {
        while (!__atomic_load_n(&allow_release, __ATOMIC_ACQUIRE)) asm volatile("nop");
    }
    if (held_kernel[id]) failed += page_free(PAGE_KERNEL, held_kernel[id]) != PAGE_OK;
    if (held_user[id]) failed += page_free(PAGE_USER, held_user[id]) != PAGE_OK;
    // All harts now repeatedly contend on both pool locks.
    for (unsigned i = 0; i < 64; i++) {
        enum page_pool pool = (i & 1) ? PAGE_USER : PAGE_KERNEL;
        uint64 *p = page_alloc(pool);
        if (!p) { failed++; continue; }
        p[0] = 0xfeed0000UL + id;
        for (unsigned delay = 0; delay < 30; delay++) asm volatile("nop");
        if (p[0] != 0xfeed0000UL + id) failed++;
        failed += page_free(pool, p) != PAGE_OK;
    }
    hart_page_failures[id] = failed;
}
#endif

static void show_ticks(void)
{
    for (unsigned id = 0; id < CPUS; id++)
        console_printf("[ticks] hart=%u count=%lu\n", id, trap_ticks(id));
}

static int run_tests(void)
{
    int failures = console_selftest();
#if LAB >= 2
    int page_fail = page_selftest();
    console_printf("[test] page %s\n", page_fail ? "FAIL" : "PASS");
    failures += page_fail;
    int vm_fail = vm_selftest();
    console_printf("[test] vm %s\n", vm_fail ? "FAIL" : "PASS");
    failures += vm_fail;
#else
    console_printf("[test] page SKIP (Lab 2)\n[test] vm SKIP (Lab 2)\n");
#endif
#if LAB >= 3
    int trap_fail = trap_selftest();
    console_printf("[test] trap %s\n", trap_fail ? "FAIL" : "PASS");
    failures += trap_fail;
#endif
    console_printf("[test] summary failures=%d\n", failures);
    return failures;
}

#if LAB >= 2
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
#endif

static void dispatch(char *line)
{
    while (*line == ' ' || *line == '\t') line++;
    char *arg = line;
    while (*arg && *arg != ' ' && *arg != '\t') arg++;
    if (*arg) *arg++ = '\0';
    while (*arg == ' ' || *arg == '\t') arg++;
    if (!*line) return;
    if (!strcmp(line, "help")) {
        console_printf("Commands: help boot mem vm [hex-address] ticks irq test echo <text> quit\n");
        console_printf("Kernel monitor in S-mode; no user processes yet. LAB=%d CPUS=%d\n", LAB, CPUS);
    } else if (!strcmp(line, "boot")) show_boot();
    else if (!strcmp(line, "test")) run_tests();
    else if (!strcmp(line, "ticks")) show_ticks();
    else if (!strcmp(line, "irq"))
        console_printf("[irq] uart_rx=%lu bytes=%lu dropped=%lu\n", trap_uart_interrupts(), uart_rx_bytes(), uart_rx_dropped());
    else if (!strcmp(line, "echo")) console_printf("[echo] %s\n", arg);
    else if (!strcmp(line, "mem")) {
#if LAB >= 2
        show_memory();
#else
        console_printf("[mem] unavailable in Lab 1\n");
#endif
    } else if (!strcmp(line, "vm")) {
#if LAB >= 2
        if (*arg) {
            uint64 address;
            if (parse_hex(arg, &address)) console_printf("[vm] invalid hexadecimal address\n");
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
#else
        console_printf("[vm] paging disabled in Lab 1\n");
#endif
    } else if (!strcmp(line, "quit")) {
        console_printf("[shutdown]\n");
        sbi_shutdown();
    } else console_printf("[command] unknown: %s (try help)\n", line);
}

static void monitor(void)
{
    char line[128];
    unsigned used = 0;
    int overflow = 0, skip_lf = 0;
    console_printf("pgos> ");
    for (;;) {
        int c = uart_getc();
        if (c < 0) {
#if LAB >= 3
            asm volatile("wfi");
#else
            asm volatile("nop");
#endif
            continue;
        }
        if (skip_lf && c == '\n') { skip_lf = 0; continue; }
        skip_lf = 0;
        if (c == '\r' || c == '\n') {
            skip_lf = c == '\r';
            console_printf("\n");
            if (overflow) console_printf("[input] line too long; discarded\n");
            else { line[used] = '\0'; dispatch(line); }
            used = 0; overflow = 0;
            console_printf("pgos> ");
        } else if (c == 8 || c == 127) {
            if (!overflow && used) { used--; console_printf("\b \b"); }
        } else if (c >= 32 && c <= 126) {
            if (overflow) continue;
            if (used == sizeof(line) - 1) { overflow = 1; continue; }
            line[used++] = (char)c;
            console_putc((char)c);
        }
    }
}

void kernel_main(uint64 id, uint64 dtb, int primary)
{
    boot_trace_mark(id, BOOT_SUPERVISOR);
    if (primary) {
        leader = id;
        console_init();
#if LAB >= 3
        plic_init();
#endif
        console_printf("\nPugeZhangOS: Lab %d, %d harts, boot hart %lu\n", LAB, CPUS, id);
#if LAB >= 2
        uint64 length = dtb_length(dtb);
        if (!length) panic("invalid firmware DTB");
        console_printf("[firmware] OpenSBI DTB=0x%lx bytes=%lu preserved\n", dtb, length);
        page_init(dtb, length);
        if (vm_init()) panic("kernel page table creation failed");
        for (int pool = 0; pool < 2; pool++) {
            struct page_stats s; page_get_stats(pool, &s); baseline_free[pool] = s.free;
        }
        int started = sbi_boot_secondaries(id, dtb, CPUS);
        console_printf("[firmware] HSM started=%d\n", started);
#else
        (void)dtb;
#endif
        boot_trace_mark(id, BOOT_GLOBAL);
        __atomic_store_n(&global_ready, 1, __ATOMIC_RELEASE);
    } else {
        while (!__atomic_load_n(&global_ready, __ATOMIC_ACQUIRE)) asm volatile("nop");
    }
#if LAB >= 2
    vm_enable();
    boot_trace_mark(id, BOOT_PAGING);
#endif
    trap_init_hart();
    boot_trace_mark(id, BOOT_TRAP);
    records[id].satp = read_satp();
    for (unsigned seq = 0; seq < 4; seq++)
        console_printf("[uart-test] hart=%lu seq=%u\n", id, seq);
#if LAB >= 2
    parallel_pages(id, primary);
#endif
    boot_trace_mark(id, BOOT_READY);
    __atomic_store_n(&records[id].ready, 1, __ATOMIC_RELEASE);
    __atomic_fetch_add(&ready_count, 1, __ATOMIC_ACQ_REL);
    if (primary) {
        wait_count(&ready_count, CPUS);
        show_boot();
#if LAB >= 2
        int failed = 0;
        for (unsigned i = 0; i < CPUS; i++) failed += hart_page_failures[i];
        for (int pool = 0; pool < 2; pool++) {
            struct page_stats s; page_get_stats(pool, &s); failed += s.free != baseline_free[pool];
        }
        console_printf("[test] page-concurrent %s\n", failed ? "FAIL" : "PASS");
        boot_failures += failed;
        show_memory();
#endif
#if LAB >= 3
        uart_enable_rx_irq();
        trap_enable();
#endif
        boot_failures += run_tests();
        if (boot_failures) panic("boot self-tests failed");
        console_printf("[ready] lab=%d cpus=%d leader=%lu\n", LAB, CPUS, leader);
        monitor();
    } else {
#if LAB >= 3
        trap_enable();
#endif
        for (;;) asm volatile("wfi");
    }
}

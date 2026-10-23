/* S-mode diagnostic command input. This is not a user-process shell. */
#include "monitor.h"
#include "selftest.h"
#include "console.h"
#include "uart.h"
#include "memory.h"
#include "platform.h"
#include "process.h"

static void dispatch(char *line)
{
    while (*line == ' ' || *line == '\t') line++;
    char *arg = line;
    while (*arg && *arg != ' ' && *arg != '\t') arg++;
    if (*arg) *arg++ = '\0';
    while (*arg == ' ' || *arg == '\t') arg++;
    if (!*line) return;
    if (!strcmp(line, "help")) {
        console_printf("Commands: help boot mem vm [hex-address] ticks irq test user [0..11] utest heap htest proc echo <text> quit\n");
        console_printf("S-mode kernel monitor; user launches a separate U-mode process. Lab 5, CPUS=%d\n", CPUS);
    } else if (!strcmp(line, "boot")) diagnostics_boot();
    else if (!strcmp(line, "mem")) diagnostics_memory();
    else if (!strcmp(line, "vm")) diagnostics_vm(arg);
    else if (!strcmp(line, "ticks")) diagnostics_ticks();
    else if (!strcmp(line, "irq")) diagnostics_irq();
    else if (!strcmp(line, "test")) selftest_run();
    else if (!strcmp(line, "user")) {
        unsigned mode = PROCESS_NORMAL;
        if (*arg) {
            mode = 0;
            for (const char *digit = arg; *digit; digit++) {
                if (*digit < '0' || *digit > '9' || mode > PROCESS_HEAP_EXEC) {
                    console_printf("[user] mode must be 0..11\n");
                    return;
                }
                mode = mode * 10 + (unsigned)(*digit - '0');
            }
            if (mode > PROCESS_HEAP_EXEC) {
                console_printf("[user] mode must be 0..11\n");
                return;
            }
        }
        struct process_result result;
        int error = process_run(mode, &result);
        if (error) console_printf("[user] creation failed: %d\n", error);
        diagnostics_process();
    }
    else if (!strcmp(line, "utest")) user_process_selftest();
    else if (!strcmp(line, "heap")) {
        struct process_result result;
        int error = process_run(PROCESS_HEAP_DEMO, &result);
        if (error) console_printf("[heap] creation failed: %d\n", error);
        diagnostics_process();
    }
    else if (!strcmp(line, "htest")) {
        int failures = user_heap_selftest();
        failures += user_heap_process_selftest();
        console_printf("[heap-check] failures=%d\n", failures);
    }
    else if (!strcmp(line, "proc")) diagnostics_process();
    else if (!strcmp(line, "echo")) console_printf("[echo] %s\n", arg);
    else if (!strcmp(line, "quit")) {
        console_printf("[shutdown]\n");
        platform_shutdown();
    } else console_printf("[command] unknown: %s (try help)\n", line);
}

void monitor_run(void)
{
    char line[128];
    unsigned used = 0;
    int overflow = 0, skip_lf = 0;
    console_printf("pgos> ");
    for (;;) {
        int c = uart_getc();
        if (c < 0) {
            asm volatile("wfi");
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

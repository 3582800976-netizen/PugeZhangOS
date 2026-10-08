/* 内核诊断台：读取串口命令、编辑输入行，再分发到各模块。 */
#include "platform.h"
#include "console.h"
#include "uart.h"
#include "memory.h"
#include "monitor.h"
#include "selftest.h"

static void dispatch(char *line)
{
    /* 在字符串原处切开命令和参数，不需要额外申请内存。 */
    while (*line == ' ' || *line == '\t') line++;
    char *arg = line;
    while (*arg && *arg != ' ' && *arg != '\t') arg++;
    if (*arg) *arg++ = '\0';
    while (*arg == ' ' || *arg == '\t') arg++;
    if (!*line) return;

    if (!strcmp(line, "help")) {
        console_printf("Commands: help boot test echo <text> quit\n");
        console_printf("Kernel monitor in S-mode; Lab 1, CPUS=%d\n", CPUS);
    } else if (!strcmp(line, "boot")) diagnostics_boot();
    else if (!strcmp(line, "test")) selftest_run();
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
        if (c < 0) { asm volatile("nop"); continue; }
        if (skip_lf && c == '\n') { skip_lf = 0; continue; }
        skip_lf = 0;
        if (c == '\r' || c == '\n') {
            skip_lf = c == '\r';
            console_printf("\n");
            if (overflow) console_printf("[input] line too long; discarded\n");
            else { line[used] = '\0'; dispatch(line); }
            used = 0;
            overflow = 0;
            console_printf("pgos> ");
        } else if (c == 8 || c == 127) {
            if (!overflow && used) { used--; console_printf("\b \b"); }
        } else if (c >= 32 && c <= 126) {
            if (overflow) continue;
            /* 保留最后一个字节给字符串结束符；超长行整行丢弃。 */
            if (used == sizeof(line) - 1) { overflow = 1; continue; }
            line[used++] = (char)c;
            console_putc((char)c);
        }
    }
}

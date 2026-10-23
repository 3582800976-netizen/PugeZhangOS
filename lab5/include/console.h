// include/console.h — 控制台输出接口

#ifndef __CONSOLE_H
#define __CONSOLE_H

#include "types.h"

void console_init(void);
void console_putc(char c);
int  console_printf(const char *fmt, ...);
int  console_snprintf(char *dst, uint64 capacity, const char *fmt, ...);
int  console_selftest(void);
void panic(const char *message) __attribute__((noreturn));

#endif /* __CONSOLE_H */

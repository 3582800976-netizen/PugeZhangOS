/* 每一位表示已真正走过一个启动阶段；未包含 Lab 2 的分页阶段。 */
#ifndef PUGE_BOOTTRACE_H
#define PUGE_BOOTTRACE_H
#include "types.h"
#define BOOT_ENTRY      1U
#define BOOT_STACK      2U
#define BOOT_SUPERVISOR 4U
#define BOOT_GLOBAL     8U
#define BOOT_TRAP      32U
#define BOOT_READY     64U
void boot_trace_mark(uint64 hartid, uint32 event);
#endif

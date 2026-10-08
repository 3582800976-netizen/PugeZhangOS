#ifndef PG_BOOTTRACE_H
#define PG_BOOTTRACE_H
#include "types.h"
#define BOOT_ENTRY      1U
#define BOOT_STACK      2U
#define BOOT_SUPERVISOR 4U
#define BOOT_GLOBAL     8U
#define BOOT_PAGING    16U
#define BOOT_TRAP      32U
#define BOOT_READY     64U
void boot_trace_mark(uint64 hartid, uint32 event);
#endif

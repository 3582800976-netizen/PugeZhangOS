#ifndef PUGE_TRAP_H
#define PUGE_TRAP_H
#include "types.h"
void trap_init_hart(void);
/* Only synchronous page faults are handled here; no hardware IRQs yet. */
int trap_test_load_fault(uint64 address);
/* Store probe requires an address that is readable (e.g. text or rodata). */
int trap_test_store_fault(uint64 address);
uint64 trap_handle(uint64 cause, uint64 pc, uint64 value);
#endif

#ifndef PUGE_TRAP_H
#define PUGE_TRAP_H
#include "types.h"
void trap_init_hart(void);
void trap_handle(uint64 cause, uint64 pc, uint64 value) __attribute__((noreturn));
#endif

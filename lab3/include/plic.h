#ifndef PUGE_PLIC_H
#define PUGE_PLIC_H
#include "types.h"
void plic_init(void);
void plic_init_hart(void);
uint32 plic_claim(void);
void plic_complete(uint32 irq);
#endif

#ifndef PUGE_SBI_H
#define PUGE_SBI_H
#include "types.h"
#define SBI_EXT_HSM  0x48534dUL
int sbi_probe_extension(uint64 extension);
long sbi_start_hart(uint64 hartid, uint64 opaque);
int sbi_boot_secondaries(uint64 primary, uint64 dtb, uint64 active_harts);
#endif

#ifndef VM_H
#define VM_H

#include "types.h"

/* A persistent teaching window: demonstrate VA != PA without editing live
 * page tables. One data page and two intermediate page tables are reserved. */
#define VM_DIAG_ALIAS 0x40000000UL

enum vm_flags {
    VM_V = 1,
    VM_R = 2,
    VM_W = 4,
    VM_X = 8,
    VM_U = 16,
    VM_G = 32,
    VM_A = 64,
    VM_D = 128
};

enum vm_result {
    VM_OK = 0,
    VM_UNMAPPED = -1,
    VM_BAD_ADDRESS = -2,
    VM_BAD_ENTRY = -3,
    VM_NO_MEMORY = -4,
    VM_ALREADY_MAPPED = -5,
    VM_INVALID_MAP = -6
};

struct vm_mapping {
    uint64 pa;               /* includes the queried virtual page offset */
    uint64 pte;              /* raw leaf PTE */
    uint64 flags;
    uint32 level;            /* 0 = 4 KiB, 1 = 2 MiB, 2 = 1 GiB */
    uint32 vpn[3];           /* vpn[2] -> vpn[1] -> vpn[0] */
};

int vm_init(void);            /* called once before publishing boot readiness */
void vm_enable(void);        /* each hart writes its own satp and flushes TLB */
uint64 vm_root_address(void);
uint64 vm_diag_frame_address(void);
int vm_lookup(uint64 va, struct vm_mapping *out);
int vm_selftest(void);

#endif

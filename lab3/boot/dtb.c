/* The DTB resides in RAM but is not free page storage. */
#include "platform.h"
#include "dtb.h"

static uint32 read_be32(const uint8 *p)
{
    return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | p[3];
}

uint64 boot_dtb_size(uint64 address)
{
    if (address < KERNEL_BASE || address > PHYS_END - 8) return 0;
    const uint8 *p = (const uint8 *)address;
    if (read_be32(p) != 0xd00dfeedU) return 0;
    uint64 size = read_be32(p + 4);
    return size >= 40 && size <= PHYS_END - address ? size : 0;
}

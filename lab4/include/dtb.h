#ifndef PUGE_DTB_H
#define PUGE_DTB_H
#include "types.h"
/* Validate firmware DTB header and return bytes to preserve, or 0 on error. */
uint64 boot_dtb_size(uint64 address);
#endif

#ifndef PAGE_H
#define PAGE_H

#include "types.h"

#define PAGE_SIZE 4096UL
#define PAGE_RAM_END 0x88000000UL

enum page_pool { PAGE_KERNEL = 0, PAGE_USER = 1 };

/* A successful free is 0; rejected requests do not change the free list. */
enum page_result {
    PAGE_OK = 0,
    PAGE_BAD_POOL = -1,
    PAGE_BAD_ALIGNMENT = -2,
    PAGE_OUTSIDE_POOL = -3,
    PAGE_ALREADY_FREE = -4,
    PAGE_RESERVED = -5
};

struct page_stats {
    uint64 base;
    uint64 limit;             /* exclusive */
    uint64 total;
    uint64 free;
    uint64 allocations;
    uint64 frees;
    uint64 rejected;
};

/* Reserve the boot DTB handed to the kernel by OpenSBI; pass 0,0 if absent.
 * Its pages are not free storage, even though they lie above _kernel_end. */
void page_init(uint64 reserve_base, uint64 reserve_len);
void *page_alloc(enum page_pool pool); /* zero-filled 4 KiB page, or 0 */
int page_free(enum page_pool pool, void *address);
void page_get_stats(enum page_pool pool, struct page_stats *out);
int page_selftest(void);              /* number of failed checks */

#endif

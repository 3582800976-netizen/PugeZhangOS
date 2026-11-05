#include "mod.h"

// 空闲链和位图共同记录每页的所有权；两段页池分别持锁。
static alloc_region_t kern_region, user_region;
#define MAX_MANAGED_PAGES (128U * 1024U * 1024U / PGSIZE)
static uint8 allocated[(MAX_MANAGED_PAGES + 7) / 8];
extern uint64 boot_dtb;
static uint64 dtb_begin, dtb_end;

static uint32 page_index(uint64 page)
{
    return (uint32)((page - kern_region.begin) / PGSIZE);
}

bool check_inkernel(uint64 page)
{
    return page >= kern_region.begin && page < kern_region.end;
}

static uint32 read_be32(const uint8 *p)
{
    return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) |
           ((uint32)p[2] << 8) | p[3];
}

static void region_init(alloc_region_t *r, uint64 begin, uint64 end, char *name)
{
    r->begin = begin;
    r->end = end;
    r->allocable = 0;
    r->list_head.next = NULL;
    spinlock_init(&r->lk, name);
    for (uint64 p = begin; p < end; p += PGSIZE) {
        if (dtb_begin < dtb_end && p < dtb_end && p + PGSIZE > dtb_begin)
            continue;
        memset((void *)p, 1, PGSIZE);
        page_node_t *node = (page_node_t *)p;
        node->next = r->list_head.next;
        r->list_head.next = node;
        r->allocable++;
    }
}

void pmem_init(void)
{
    uint64 begin = ALIGN_UP((uint64)ALLOC_BEGIN, PGSIZE);
    uint64 end = ALIGN_DOWN((uint64)ALLOC_END, PGSIZE);
    assert(end > begin && (end - begin) / PGSIZE <= MAX_MANAGED_PAGES &&
           end - begin > (uint64)KERN_PAGES * PGSIZE, "pmem_init: invalid range");
    assert(KERN_PAGES % 8 == 0, "pmem_init: bitmap pool alignment");
    memset(allocated, 0, sizeof(allocated));
    dtb_begin = dtb_end = 0;
#ifndef VISIONFIVE2
    // QEMU 可能把 DTB 放进可分配 RAM：只读取有效范围内的头部并保留它。
    if (boot_dtb >= begin && boot_dtb <= end - 8) {
        const uint8 *dtb = (const uint8 *)boot_dtb;
        uint32 size = read_be32(dtb + 4);
        assert(read_be32(dtb) == 0xd00dfeed && size >= 40 &&
               size <= end - boot_dtb, "pmem_init: bad DTB");
        dtb_begin = boot_dtb;
        dtb_end = boot_dtb + size;
    } else {
        assert(boot_dtb == 0 || boot_dtb < KERNEL_BASE || boot_dtb >= end,
               "pmem_init: DTB overlaps kernel");
    }
#else
    // VF2 课程前提要求 DTB 在整个管理窗口之外；暂不猜测板卡地址。
    assert(boot_dtb != 0 && boot_dtb >= end, "pmem_init: VF2 DTB outside managed window required");
#endif
    region_init(&kern_region, begin, begin + (uint64)KERN_PAGES * PGSIZE, "kernel pages");
    region_init(&user_region, kern_region.end, end, "user pages");
    assert(kern_region.allocable && user_region.allocable, "pmem_init: empty pool");
}

void *pmem_alloc(bool in_kernel)
{
    alloc_region_t *r = in_kernel ? &kern_region : &user_region;
    spinlock_acquire(&r->lk);
    page_node_t *page = r->list_head.next;
    if (page == NULL) {
        spinlock_release(&r->lk);
        return NULL;
    }
    r->list_head.next = page->next;
    r->allocable--;
    uint32 i = page_index((uint64)page);
    allocated[i / 8] |= (uint8)(1U << (i % 8));
    spinlock_release(&r->lk);
    memset(page, 0, PGSIZE);
    return page;
}

void pmem_free(uint64 page, bool in_kernel)
{
    alloc_region_t *r = in_kernel ? &kern_region : &user_region;
    assert(page >= r->begin && page < r->end && page % PGSIZE == 0,
           "pmem_free: wrong region or alignment");
    spinlock_acquire(&r->lk);
    uint32 i = page_index(page);
    if (!(allocated[i / 8] & (1U << (i % 8)))) {
        spinlock_release(&r->lk);
        panic("pmem_free: double free");
    }
    allocated[i / 8] &= (uint8)~(1U << (i % 8));
    memset((void *)page, 1, PGSIZE);
    page_node_t *node = (page_node_t *)page;
    node->next = r->list_head.next;
    r->list_head.next = node;
    r->allocable++;
    spinlock_release(&r->lk);
}

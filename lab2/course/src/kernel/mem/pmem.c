#include "mod.h"

static alloc_region_t kern_region, user_region;
#define MAX_PAGES (128U * 1024U * 1024U / PGSIZE)
static uint8 allocated[(MAX_PAGES + 7) / 8];
extern uint64 boot_dtb;
static uint64 dtb_begin, dtb_end;

static uint32 index_of(uint64 p) { return (uint32)((p - (uint64)ALLOC_BEGIN) / PGSIZE); }
static uint32 be32(const uint8 *p)
{
    return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | p[3];
}
static void region_init(alloc_region_t *r, uint64 begin, uint64 end, char *name)
{
    r->begin = begin; r->end = end; r->allocable = 0; r->list_head.next = NULL;
    spinlock_init(&r->lk, name);
    for (uint64 p = begin; p < end; p += PGSIZE) {
        if (dtb_begin < dtb_end && p < dtb_end && p + PGSIZE > dtb_begin) continue;
        page_node_t *node = (page_node_t *)p;
        node->next = r->list_head.next; r->list_head.next = node; r->allocable++;
    }
}
void pmem_init(void)
{
    uint64 begin = (uint64)ALLOC_BEGIN, end = (uint64)ALLOC_END;
    assert(begin % PGSIZE == 0 && end % PGSIZE == 0 && end > begin, "pmem layout");
    assert((end - begin) / PGSIZE <= MAX_PAGES && end - begin > KERN_PAGES * PGSIZE, "pmem range");
    // 首先限制到内核映像后的 RAM，避免访问受保护的 OpenSBI 区域。
    if (boot_dtb >= begin && boot_dtb <= end - 8) {
        const uint8 *dtb = (const uint8 *)boot_dtb;
        uint32 size = be32(dtb + 4);
        if (be32(dtb) == 0xd00dfeed && size >= 40 && size <= end - boot_dtb) {
            dtb_begin = boot_dtb; dtb_end = boot_dtb + size;
        } else panic("bad DTB");
    } else panic("DTB outside safe RAM");
    region_init(&kern_region, begin, begin + KERN_PAGES * PGSIZE, "kernel pages");
    region_init(&user_region, kern_region.end, end, "user pages");
}
void *pmem_alloc(bool in_kernel)
{
    alloc_region_t *r = in_kernel ? &kern_region : &user_region;
    spinlock_acquire(&r->lk);
    page_node_t *page = r->list_head.next;
    if (!page) { spinlock_release(&r->lk); panic("physical page exhausted"); }
    r->list_head.next = page->next; r->allocable--;
    uint32 i = index_of((uint64)page); allocated[i / 8] |= (uint8)(1U << (i % 8));
    spinlock_release(&r->lk);
    memset(page, 0, PGSIZE);
    return page;
}
void pmem_free(uint64 page, bool in_kernel)
{
    alloc_region_t *r = in_kernel ? &kern_region : &user_region;
    assert(page >= r->begin && page < r->end && page % PGSIZE == 0, "physical page wrong region or alignment");
    spinlock_acquire(&r->lk);
    uint32 i = index_of(page);
    if (!(allocated[i / 8] & (1U << (i % 8)))) { spinlock_release(&r->lk); panic("physical page double free"); }
    allocated[i / 8] &= (uint8)~(1U << (i % 8));
    page_node_t *node = (page_node_t *)page;
    node->next = r->list_head.next; r->list_head.next = node; r->allocable++;
    spinlock_release(&r->lk);
}

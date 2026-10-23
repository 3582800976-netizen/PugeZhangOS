/* The process owner serializes its interval list and VM. Only the fixed
 * metadata repository is shared between harts; never call VM with its lock. */
#include "user_heap.h"
#include "process.h"
#include "user_layout.h"
#include "page.h"
#include "spinlock.h"
#include "memory.h"

static mmap_region_node_t list_mmap_region_node[N_MMAP];
static mmap_region_node_t *available;
static struct spinlock repository_lock;
static uint64 available_count;
static int initialized;

void user_heap_pool_init(void)
{
    spin_init(&repository_lock);
    memset(list_mmap_region_node, 0, sizeof list_mmap_region_node);
    for (unsigned i = 0; i < N_MMAP; i++)
        list_mmap_region_node[i].next = i + 1 < N_MMAP ?
                                      &list_mmap_region_node[i + 1] : 0;
    available = &list_mmap_region_node[0];
    available_count = N_MMAP;
    initialized = 1;
}

static mmap_region_t *take_node(void)
{
    if (!initialized)
        return 0;
    spin_acquire(&repository_lock);
    mmap_region_node_t *node = available;
    if (node) {
        available = node->next;
        --available_count;
    }
    spin_release(&repository_lock);
    if (node) {
        memset(&node->mmap, 0, sizeof node->mmap);
        node->next = 0;
    }
    return node ? &node->mmap : 0;
}

static void return_node(mmap_region_t *region)
{
    /* mmap is the first member of the repository wrapper. A node is private
     * to its process until it is published back under this short lock. */
    mmap_region_node_t *node = (mmap_region_node_t *)region;
    memset(&node->mmap, 0, sizeof node->mmap);
    spin_acquire(&repository_lock);
    node->next = available;
    available = node;
    ++available_count;
    spin_release(&repository_lock);
}

uint64 user_heap_nodes_free(void)
{
    if (!initialized)
        return 0;
    spin_acquire(&repository_lock);
    uint64 count = available_count;
    spin_release(&repository_lock);
    return count;
}

int user_heap_create(struct process *p)
{
    if (!p || p->heap_initialized || p->mmap)
        return SYS_EINVAL;
    mmap_region_t *region = take_node();
    if (!region)
        return SYS_ENOMEM;
    region->begin = USER_HEAP_BASE;
    region->npages = USER_HEAP_PAGES;
    p->mmap = region;
    p->heap_initialized = 1;
    return 0;
}

static int valid_range(uint64 begin, uint64 npages)
{
    return npages && !(begin & (PAGE_SIZE - 1)) &&
           begin >= USER_HEAP_BASE && begin < USER_HEAP_LIMIT &&
           npages <= (USER_HEAP_LIMIT - begin) / PAGE_SIZE;
}

static uint64 region_end(const mmap_region_t *region)
{
    return region->begin + region->npages * PAGE_SIZE;
}

static int vm_error(int result)
{
    return result == VM_NO_MEMORY ? SYS_ENOMEM : SYS_EINVAL;
}

int user_heap_alloc(struct process *p, uint64 hint, uint64 npages,
                    uint64 *out, int budget)
{
    if (!p || !p->heap_initialized || !out || budget < -1 || !npages ||
        npages > USER_HEAP_PAGES || (hint && !valid_range(hint, npages)))
        return SYS_EINVAL;

    mmap_region_t *previous = 0, *region = p->mmap;
    uint64 begin = 0;
    for (; region; previous = region, region = region->next) {
        if (!hint) {
            if (region->npages >= npages) {
                begin = region->begin;
                break;
            }
        } else if (hint >= region->begin && hint < region_end(region) &&
                   npages <= (region_end(region) - hint) / PAGE_SIZE) {
            begin = hint;
            break;
        }
    }
    if (!region)
        return SYS_ENOMEM;

    uint64 end = begin + npages * PAGE_SIZE;
    uint64 left = (begin - region->begin) / PAGE_SIZE;
    uint64 right = (region_end(region) - end) / PAGE_SIZE;
    mmap_region_t *split = 0;
    if (left && right) {
        split = take_node();
        if (!split)
            return SYS_ENOMEM;
    }

    int result = user_vm_heap_alloc(&p->vm, begin, npages, budget);
    if (result != VM_OK) {
        if (split)
            return_node(split);
        return vm_error(result);
    }

    /* Mapping committed. No operation below can fail. */
    if (!left && !right) {
        if (previous)
            previous->next = region->next;
        else
            p->mmap = region->next;
        return_node(region);
    } else if (!left) {
        region->begin = end;
        region->npages = right;
    } else if (!right) {
        region->npages = left;
    } else {
        split->begin = end;
        split->npages = right;
        split->next = region->next;
        region->npages = left;
        region->next = split;
    }
    *out = begin;
    return 0;
}

int user_heap_free(struct process *p, uint64 begin, uint64 npages)
{
    if (!p || !p->heap_initialized || !valid_range(begin, npages))
        return SYS_EINVAL;
    uint64 end = begin + npages * PAGE_SIZE;
    mmap_region_t *previous = 0, *next = p->mmap;
    while (next && next->begin < begin) {
        previous = next;
        next = next->next;
    }
    if ((previous && region_end(previous) > begin) ||
        (next && next->begin < end))
        return SYS_EINVAL;
    int join_left = previous && region_end(previous) == begin;
    int join_right = next && next->begin == end;
    mmap_region_t *insert = 0;
    if (!join_left && !join_right) {
        insert = take_node();
        if (!insert)
            return SYS_ENOMEM;
    }

    /* VM validates the complete range (including ownership) before clearing
     * its first PTE. A rejection therefore leaves this list untouched. */
    int result = user_vm_heap_free(&p->vm, begin, npages);
    if (result != VM_OK) {
        if (insert)
            return_node(insert);
        return vm_error(result);
    }
    if (join_left && join_right) {
        previous->npages += npages + next->npages;
        previous->next = next->next;
        return_node(next);
    } else if (join_left) {
        previous->npages += npages;
    } else if (join_right) {
        next->begin = begin;
        next->npages += npages;
    } else {
        insert->begin = begin;
        insert->npages = npages;
        insert->next = next;
        if (previous)
            previous->next = insert;
        else
            p->mmap = insert;
    }
    return 0;
}

int user_heap_snapshot(const struct process *p, struct user_heap_snapshot *out)
{
    if (!p || !p->heap_initialized || !out)
        return SYS_EINVAL;
    memset(out, 0, sizeof *out);
    out->info.base = USER_HEAP_BASE;
    out->info.limit = USER_HEAP_LIMIT;
    out->info.total_pages = USER_HEAP_PAGES;
    for (const mmap_region_t *region = p->mmap; region; region = region->next) {
        uint64 index = out->info.regions++;
        out->regions[index].begin = region->begin;
        out->regions[index].npages = region->npages;
        out->info.free_pages += region->npages;
        if (region->npages > out->info.largest_free_pages)
            out->info.largest_free_pages = region->npages;
    }
    out->info.allocated_pages = USER_HEAP_PAGES - out->info.free_pages;
    out->info.node_free = user_heap_nodes_free();
    return 0;
}

void user_heap_destroy(struct process *p)
{
    if (!p)
        return;
    mmap_region_t *region = p->mmap;
    p->mmap = 0;
    p->heap_initialized = 0;
    while (region) {
        mmap_region_t *next = region->next;
        return_node(region);
        region = next;
    }
}

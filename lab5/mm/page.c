/* Lab 2: two physical page pools, with explicit ownership checks.
 *
 * The free-list link lives in a free page itself. An independent bitmap
 * records whether each page has been handed to a caller: this lets us reject
 * accidental double frees before they can corrupt the linked list.
 */
#include "page.h"
#include "spinlock.h"
#include "memory.h"
#include "console.h"

#define RAM_START 0x80000000UL
#define RAM_PAGE_COUNT ((PAGE_RAM_END - RAM_START) / PAGE_SIZE)

extern char _kernel_end[];

struct free_page { struct free_page *next; };
struct pool_state {
    struct spinlock lock;
    struct free_page head; /* sentinel; head itself is never allocated */
    struct page_stats stats;
};

static struct pool_state pools[2];
/* Pools share this array, but own disjoint byte ranges. Pool boundaries are
 * rounded to 8 pages so a bitmap byte is never touched under two locks. */
static uint8 owned[RAM_PAGE_COUNT / 8];
static uint64 reserved_begin, reserved_end;

static uint64 round_up(uint64 value, uint64 alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static uint64 page_index(uint64 address)
{
    return (address - RAM_START) / PAGE_SIZE;
}

static int is_owned(uint64 address)
{
    uint64 index = page_index(address);
    return (owned[index / 8] & (1U << (index % 8))) != 0;
}

static void set_owned(uint64 address, int allocated)
{
    uint64 index = page_index(address);
    uint8 mask = (uint8)(1U << (index % 8));
    if (allocated)
        owned[index / 8] |= mask;
    else
        owned[index / 8] &= (uint8)~mask;
}

static int valid_pool(enum page_pool pool)
{
    return pool == PAGE_KERNEL || pool == PAGE_USER;
}

static int reserved(uint64 address)
{
    return address >= reserved_begin && address < reserved_end;
}

static void prepare_pool(enum page_pool id, uint64 base, uint64 limit)
{
    struct pool_state *p = &pools[id];
    spin_init(&p->lock);
    p->head.next = 0;
    p->stats.base = base;
    p->stats.limit = limit;
    p->stats.total = p->stats.free = 0;
    p->stats.allocations = p->stats.frees = p->stats.rejected = 0;
    /* Descending insertion makes allocation begin at the lowest free page. */
    for (uint64 address = limit; address > base;) {
        address -= PAGE_SIZE;
        if (reserved(address))
            continue;
        struct free_page *node = (struct free_page *)address;
        node->next = p->head.next;
        p->head.next = node;
        p->stats.total++;
        p->stats.free++;
    }
}

void page_init(uint64 reserve_base, uint64 reserve_len)
{
    uint64 base = round_up((uint64)_kernel_end, PAGE_SIZE);
    /* A bad linker layout must produce empty pools, not an unsigned-length
     * wraparound followed by writes outside RAM. vm_init reports the error. */
    if (base < 0x80200000UL || base >= PAGE_RAM_END)
        base = PAGE_RAM_END;
    uint64 count = (PAGE_RAM_END - base) / PAGE_SIZE;
    uint64 split = base + (count / 2) * PAGE_SIZE;
    /* The bitmap shares bytes for groups of 8 pages. Align the split to a
     * byte boundary so the two pool locks also protect independent bytes. */
    split &= ~(8 * PAGE_SIZE - 1);
    reserved_begin = reserved_end = 0;
    if (reserve_len && reserve_base < PAGE_RAM_END &&
        reserve_base + reserve_len >= reserve_base &&
        reserve_base + reserve_len <= PAGE_RAM_END) {
        reserved_begin = reserve_base & ~(PAGE_SIZE - 1);
        reserved_end = round_up(reserve_base + reserve_len, PAGE_SIZE);
    }
    memset(owned, 0, sizeof owned);
    prepare_pool(PAGE_KERNEL, base, split);
    prepare_pool(PAGE_USER, split, PAGE_RAM_END);
}

void *page_alloc(enum page_pool id)
{
    if (!valid_pool(id))
        return 0;
    struct pool_state *p = &pools[id];
    spin_acquire(&p->lock);
    struct free_page *node = p->head.next;
    if (node) {
        p->head.next = node->next;
        set_owned((uint64)node, 1);
        p->stats.free--;
        p->stats.allocations++;
    }
    spin_release(&p->lock);
    if (node)
        memset(node, 0, PAGE_SIZE);
    return node;
}

int page_free(enum page_pool id, void *address)
{
    if (!valid_pool(id))
        return PAGE_BAD_POOL;
    struct pool_state *p = &pools[id];
    uint64 a = (uint64)address;
    spin_acquire(&p->lock);
    int result = PAGE_OK;
    if (a % PAGE_SIZE)
        result = PAGE_BAD_ALIGNMENT;
    else if (a < p->stats.base || a >= p->stats.limit)
        result = PAGE_OUTSIDE_POOL;
    else if (reserved(a))
        result = PAGE_RESERVED;
    else if (!is_owned(a))
        result = PAGE_ALREADY_FREE;

    if (result != PAGE_OK) {
        p->stats.rejected++;
    } else {
        /* Poison released storage to make stale references easier to spot.
         * The first word is then reused for the free-list link. */
        memset(address, 0xdd, PAGE_SIZE);
        struct free_page *node = address;
        node->next = p->head.next;
        p->head.next = node;
        set_owned(a, 0);
        p->stats.free++;
        p->stats.frees++;
    }
    spin_release(&p->lock);
    return result;
}

void page_get_stats(enum page_pool id, struct page_stats *out)
{
    if (!out)
        return;
    if (!valid_pool(id)) {
        memset(out, 0, sizeof *out);
        return;
    }
    struct pool_state *p = &pools[id];
    spin_acquire(&p->lock);
    *out = p->stats;
    spin_release(&p->lock);
}

static int check(const char *name, int passed)
{
    console_printf("[%s] lab2 page %s\n", passed ? "PASS" : "FAIL", name);
    return !passed;
}

int page_selftest(void)
{
    int failures = 0;
    for (int pool = PAGE_KERNEL; pool <= PAGE_USER; pool++) {
        void *pages[8] = {0};
        struct page_stats before, after;
        page_get_stats(pool, &before);
        int unique = 1, zeroed = 1, intact = 1;
        for (int i = 0; i < 8; i++) {
            pages[i] = page_alloc(pool);
            if (!pages[i] || (uint64)pages[i] % PAGE_SIZE) {
                unique = 0;
                continue;
            }
            if ((uint64)pages[i] < before.base || (uint64)pages[i] >= before.limit)
                unique = 0;
            for (int j = 0; j < i; j++)
                if (pages[i] == pages[j])
                    unique = 0;
            uint8 *bytes = pages[i];
            for (uint64 j = 0; j < PAGE_SIZE; j++)
                if (bytes[j] != 0)
                    zeroed = 0;
            memset(pages[i], i + 1, PAGE_SIZE);
        }
        for (int i = 0; i < 8; i++) {
            if (!pages[i]) {
                intact = 0;
                continue;
            }
            uint8 *bytes = pages[i];
            for (uint64 j = 0; j < PAGE_SIZE; j++)
                if (bytes[j] != i + 1)
                    intact = 0;
        }
        failures += check(pool == PAGE_KERNEL ? "kernel uniqueness/alignment" :
                                                  "user uniqueness/alignment", unique);
        failures += check("zero-on-allocation and independent contents", zeroed && intact);
        if (pages[0]) {
            failures += check("reject wrong pool", page_free(1 - pool, pages[0]) == PAGE_OUTSIDE_POOL);
            failures += check("reject misaligned address", page_free(pool, (uint8 *)pages[0] + 1) == PAGE_BAD_ALIGNMENT);
        }
        int released = 1;
        for (int i = 0; i < 8; i++)
            if (pages[i] && page_free(pool, pages[i]) != PAGE_OK)
                released = 0;
        failures += check("release all allocated pages", released);
        if (pages[7]) {
            failures += check("reject double free", page_free(pool, pages[7]) == PAGE_ALREADY_FREE);
            void *again = page_alloc(pool);
            failures += check("reuse released page", again == pages[7]);
            if (again)
                page_free(pool, again);
        }
        page_get_stats(pool, &after);
        failures += check("free count restored", after.free == before.free);
    }
    failures += check("reject kernel image address", page_free(PAGE_KERNEL, (void *)0x80200000UL) == PAGE_OUTSIDE_POOL);
    failures += check("reject invalid pool", page_free((enum page_pool)2, (void *)0) == PAGE_BAD_POOL);
    failures += check("reject one-past-RAM address", page_free(PAGE_USER, (void *)PAGE_RAM_END) == PAGE_OUTSIDE_POOL);
    if (reserved_begin >= (uint64)_kernel_end && reserved_begin < PAGE_RAM_END) {
        enum page_pool id = reserved_begin < pools[PAGE_USER].stats.base ? PAGE_KERNEL : PAGE_USER;
        failures += check("preserve firmware DTB", page_free(id, (void *)reserved_begin) == PAGE_RESERVED);
    }
    return failures;
}

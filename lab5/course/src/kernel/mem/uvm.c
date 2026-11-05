#include "mod.h"

/*--------------------part-1: 关于内核空间<->用户空间的数据传递--------------------*/

// 用户态地址空间[src, src+len) 拷贝至 内核态地址空间[dst, dst+len)
// 注意: src dst 不一定是 page-aligned
// 检查用户叶页的权限，并取得包含页内偏移的物理地址。
static int user_address(pgtbl_t pgtbl, uint64 va, int permission, uint64 *pa)
{
    if (pgtbl == NULL || va >= TRAPFRAME)
        return -1;
    pte_t *pte = vm_getpte(pgtbl, va, false);
    if (pte == NULL || (*pte & (PTE_V | PTE_U | permission)) !=
                       (uint64)(PTE_V | PTE_U | permission) || PTE_CHECK(*pte))
        return -1;
    *pa = PTE_TO_PA(*pte) + (va & (PGSIZE - 1));
    return 0;
}

int uvm_copyin(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 len)
{
    while (len > 0) {
        uint64 pa;
        if (user_address(pgtbl, src, PTE_R, &pa) < 0)
            return -1;
        uint32 count = PGSIZE - (uint32)(src & (PGSIZE - 1));
        if (count > len) count = len;
        memmove((void *)dst, (const void *)pa, count);
        src += count;
        dst += count;
        len -= count;
    }
    return 0;
}

// 内核态地址空间[src, src+len） 拷贝至 用户态地址空间[dst, dst+len)
// 注意: src dst 不一定是 page-aligned
int uvm_copyout(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 len)
{
    while (len > 0) {
        uint64 pa;
        if (user_address(pgtbl, dst, PTE_W, &pa) < 0)
            return -1;
        uint32 count = PGSIZE - (uint32)(dst & (PGSIZE - 1));
        if (count > len) count = len;
        memmove((void *)pa, (const void *)src, count);
        dst += count;
        src += count;
        len -= count;
    }
    return 0;
}

// 用户态字符串拷贝到内核态
// 最多拷贝maxlen字节, 中途遇到'\0'则终止
// 注意: src dst 不一定是 page-aligned
int uvm_copyin_str(pgtbl_t pgtbl, uint64 dst, uint64 src, uint32 maxlen)
{
    while (maxlen > 0) {
        uint64 pa;
        if (user_address(pgtbl, src, PTE_R, &pa) < 0)
            return -1;
        uint32 count = PGSIZE - (uint32)(src & (PGSIZE - 1));
        if (count > maxlen) count = maxlen;
        const char *input = (const char *)pa;
        char *output = (char *)dst;
        for (uint32 i = 0; i < count; i++) {
            output[i] = input[i];
            if (input[i] == '\0')
                return 0;
        }
        src += count;
        dst += count;
        maxlen -= count;
    }
    return -1;
}

/* 用户页表修改事务。调用者独占页表：中间表和用户页的每次发布都
 * 有逆序日志，失败时恢复原 PTE（含非零无效项）并归还全部新页。 */
struct uvm_change {
    pte_t *slot;
    pte_t old;
    uint64 page;
    uint64 va;
    bool leaf;
};
#define UVM_CHANGES_PER_PAGE ((PGSIZE - 2 * sizeof(uint64)) / sizeof(struct uvm_change))
struct uvm_journal {
    struct uvm_journal *previous;
    uint64 count;
    struct uvm_change changes[UVM_CHANGES_PER_PAGE];
};
struct uvm_tx {
    pgtbl_t root;
    struct uvm_journal *journal;
};

static int uvm_tx_reserve(struct uvm_tx *tx)
{
    if (tx->journal != NULL && tx->journal->count < UVM_CHANGES_PER_PAGE)
        return 0;
    struct uvm_journal *page = pmem_alloc(true);
    if (page == NULL) return -1;
    page->previous = tx->journal;
    page->count = 0;
    tx->journal = page;
    return 0;
}

static void uvm_tx_finish(struct uvm_tx *tx, bool rollback)
{
    struct uvm_journal *journal = tx->journal;
    while (journal != NULL) {
        struct uvm_journal *previous = journal->previous;
        if (rollback) {
            while (journal->count != 0) {
                struct uvm_change *c = &journal->changes[--journal->count];
                if (c->leaf) {
                    // 页表只由本次调用修改；先失效本核 TLB 再归还数据页。
                    vm_unmappages(tx->root, c->va, PGSIZE, false);
                    *c->slot = c->old;
                    pmem_free(c->page, false);
                } else {
                    *c->slot = c->old;
                    pmem_free(c->page, true);
                }
            }
        }
        pmem_free((uint64)journal, true);
        journal = previous;
    }
    tx->journal = NULL;
}

static pte_t *uvm_tx_leaf(struct uvm_tx *tx, uint64 va)
{
    pgtbl_t table = tx->root;
    if (table == NULL || va >= VA_MAX) return NULL;
    for (int level = 2; level > 0; level--) {
        pte_t *slot = &table[VA_TO_VPN(va, level)];
        if (*slot & PTE_V) {
            if (!PTE_CHECK(*slot)) {
                panic("uvm_tx_leaf: unexpected upper-level leaf");
                return NULL;
            }
            table = (pgtbl_t)PTE_TO_PA(*slot);
            continue;
        }
        if (uvm_tx_reserve(tx) < 0) return NULL;
        pgtbl_t page = pmem_alloc(true);
        if (page == NULL) return NULL;
        struct uvm_change *change = &tx->journal->changes[tx->journal->count++];
        *change = (struct uvm_change){slot, *slot, (uint64)page, 0, false};
        *slot = PA_TO_PTE(page) | PTE_V;
        table = page;
    }
    return &table[VA_TO_VPN(va, 0)];
}

// src 非 NULL 时复制一个用户页；否则使用分配器清零后的页。
static int uvm_tx_map_page(struct uvm_tx *tx, uint64 va, int perm, const void *src)
{
    pte_t *slot = uvm_tx_leaf(tx, va);
    if (slot == NULL || (*slot & PTE_V) || uvm_tx_reserve(tx) < 0) return -1;
    void *page = pmem_alloc(false);
    if (page == NULL) return -1;
    if (src != NULL) memmove(page, src, PGSIZE);
    pte_t old = *slot;
    // 中间页已经准备好；正常情况下此处不再分配资源。
    if (vm_mappages(tx->root, va, (uint64)page, PGSIZE, perm) < 0) {
        pmem_free((uint64)page, false);
        return -1;
    }
    struct uvm_change *change = &tx->journal->changes[tx->journal->count++];
    *change = (struct uvm_change){slot, old, (uint64)page, va, true};
    return 0;
}

static bool uvm_valid_perm(int perm)
{
    return !(perm & ~(PTE_R | PTE_W | PTE_X | PTE_U)) &&
           (perm & (PTE_R | PTE_X)) && (!(perm & PTE_W) || (perm & PTE_R));
}

/*--------------------part-2: mmap_region相关--------------------*/

// 打印以mmap为首的mmap链
// for debug
void uvm_show_mmaplist(mmap_region_t *mmap)
{
    mmap_region_t *tmp = mmap;
    printf("\nalloced mmap_space:\n");
    if (tmp == NULL)
        printf("empty\n");
    while (tmp != NULL)
    {
        printf("alloced mmap_region: %p ~ %p\n", tmp->begin, tmp->begin + tmp->npages * PGSIZE);
        tmp = tmp->next;
    }
}

// 寻找一块足够大的区域(len), 作为 mmap_region
// 由uvm_mmap调用(处理begin==0的情况)
// 成功返回begin, 失败返回0
static bool uvm_valid_mmap_chain(mmap_region_t *head)
{
    uint64 previous_end = MMAP_BEGIN;
    unsigned count = 0;
    for (mmap_region_t *r = head; r != NULL; r = r->next) {
        if (++count > N_MMAP || r->npages == 0 ||
            (r->begin & (PGSIZE - 1)) || r->begin < previous_end ||
            r->begin >= MMAP_END || !(r->perm & PTE_U) ||
            !uvm_valid_perm(r->perm) ||
            (uint64)r->npages > (MMAP_END - r->begin) / PGSIZE)
            return false;
        previous_end = r->begin + (uint64)r->npages * PGSIZE;
    }
    return true;
}

static uint64 uvm_mmap_find(mmap_region_t *head_mmap, uint64 len, mmap_region_t **p_last_mmap, mmap_region_t **p_tmp_mmap)
{
    if (len == 0 || len > MMAP_END - MMAP_BEGIN || (len & (PGSIZE - 1)) ||
        p_last_mmap == NULL || p_tmp_mmap == NULL) return 0;
    uint64 cursor = MMAP_BEGIN;
    mmap_region_t *prev = NULL;
    unsigned count = 0;
    for (mmap_region_t *region = head_mmap; region != NULL; region = region->next) {
        if (++count > N_MMAP || region->npages == 0 ||
            (region->begin & (PGSIZE - 1)) || region->begin < cursor ||
            region->begin >= MMAP_END ||
            (uint64)region->npages > (MMAP_END - region->begin) / PGSIZE)
            return 0;
        if (len <= region->begin - cursor) {
            *p_last_mmap = prev;
            *p_tmp_mmap = region;
            return cursor;
        }
        cursor = region->begin + (uint64)region->npages * PGSIZE;
        prev = region;
    }
    if (len > MMAP_END - cursor) return 0;
    *p_last_mmap = prev;
    *p_tmp_mmap = NULL;
    return cursor;
}

// 在用户页表和进程mmap链里新增mmap区域 [begin, begin + npages * PGSIZE)
// 调用者保证begin是page-aligned的, 页面权限为perm
// 注意: 如果start==0, 意味着需要内核自主找一块足够大的空间
// 成功返回地址；无效参数、冲突或资源耗尽返回 0，不发布部分结果。
uint64 uvm_mmap(uint64 begin, uint32 npages, int perm)
{
    proc_t *p = myproc();
    if (p == NULL || p->pgtbl == NULL || npages == 0 ||
        (uint64)npages > (MMAP_END - MMAP_BEGIN) / PGSIZE ||
        !uvm_valid_perm(perm | PTE_U) || !uvm_valid_mmap_chain(p->mmap)) return 0;
    perm |= PTE_U;
    uint64 len = (uint64)npages * PGSIZE;
    mmap_region_t *prev = NULL, *next = NULL;
    if (begin == 0) {
        begin = uvm_mmap_find(p->mmap, len, &prev, &next);
        if (begin == 0) return 0;
    } else {
        if ((begin & (PGSIZE - 1)) || begin < MMAP_BEGIN ||
            begin >= MMAP_END || len > MMAP_END - begin) return 0;
        next = p->mmap;
        unsigned count = 0;
        uint64 previous_end = MMAP_BEGIN;
        while (next != NULL && next->begin < begin) {
            if (++count > N_MMAP || next->npages == 0 ||
                (next->begin & (PGSIZE - 1)) || next->begin < previous_end ||
                next->begin >= MMAP_END ||
                (uint64)next->npages > (MMAP_END - next->begin) / PGSIZE)
                return 0;
            previous_end = next->begin + (uint64)next->npages * PGSIZE;
            prev = next;
            next = next->next;
        }
        if (begin < previous_end || (next != NULL && len > next->begin - begin))
            return 0;
    }
    uint64 end = begin + len;
    bool merge_left = prev != NULL && prev->perm == perm &&
                      prev->begin + (uint64)prev->npages * PGSIZE == begin;
    bool merge_right = next != NULL && next->perm == perm && next->begin == end;
    mmap_region_t *node = NULL;
    if (!merge_left && !merge_right) {
        node = mmap_region_alloc();
        if (node == NULL) return 0;
    }
    struct uvm_tx tx = {p->pgtbl, NULL};
    for (uint64 va = begin; va < end; va += PGSIZE) {
        if (uvm_tx_map_page(&tx, va, perm, NULL) < 0) {
            uvm_tx_finish(&tx, true);
            if (node != NULL) mmap_region_free(node);
            return 0;
        }
    }
    uvm_tx_finish(&tx, false);
    if (merge_left && merge_right) {
        prev->npages += npages + next->npages;
        prev->next = next->next;
        mmap_region_free(next);
    } else if (merge_left) {
        prev->npages += npages;
    } else if (merge_right) {
        next->begin = begin;
        next->npages += npages;
    } else {
        node->begin = begin;
        node->npages = npages;
        node->perm = perm;
        node->next = next;
        if (prev != NULL) prev->next = node;
        else p->mmap = node;
    }
    return begin;
}

// 在用户页表和进程mmap链里释放mmap区域 [begin, begin + npages * PGSIZE)
// 整段必须有映射；包含空洞或缺少分割节点时保持原样。
int uvm_munmap(uint64 begin, uint32 npages)
{
    proc_t *p = myproc();
    if (p == NULL || p->pgtbl == NULL || npages == 0 ||
        (begin & (PGSIZE - 1)) || begin < MMAP_BEGIN || begin >= MMAP_END ||
        (uint64)npages > (MMAP_END - begin) / PGSIZE ||
        !uvm_valid_mmap_chain(p->mmap)) return -1;
    uint64 end = begin + (uint64)npages * PGSIZE;
    mmap_region_t *region = p->mmap;
    unsigned count = 0;
    while (region != NULL && region->begin + (uint64)region->npages * PGSIZE <= begin) {
        if (++count > N_MMAP || region->npages == 0) return -1;
        region = region->next;
    }
    if (region == NULL || begin < region->begin) return -1;
    mmap_region_t *first = region;
    uint64 cursor = begin;
    while (cursor < end) {
        if (region == NULL || ++count > N_MMAP || region->npages == 0 ||
            region->begin > cursor || region->begin >= MMAP_END ||
            (region->begin & (PGSIZE - 1)) ||
            (uint64)region->npages > (MMAP_END - region->begin) / PGSIZE)
            return -1;
        uint64 region_end = region->begin + (uint64)region->npages * PGSIZE;
        if (region_end <= cursor) return -1;
        cursor = region_end < end ? region_end : end;
        region = region->next;
    }
    for (uint64 va = begin; va < end; va += PGSIZE) {
        pte_t *slot = vm_getpte(p->pgtbl, va, false);
        if (slot == NULL || (*slot & (PTE_V | PTE_U)) != (PTE_V | PTE_U) ||
            PTE_CHECK(*slot)) return -1;
    }
    uint64 first_end = first->begin + (uint64)first->npages * PGSIZE;
    mmap_region_t *split = NULL;
    if (begin > first->begin && end < first_end) {
        split = mmap_region_alloc();
        if (split == NULL) return -1;
    }
    vm_unmappages(p->pgtbl, begin, (uint64)npages * PGSIZE, true);
    mmap_region_t **link = &p->mmap;
    while (*link != NULL) {
        mmap_region_t *cur = *link;
        uint64 cur_end = cur->begin + (uint64)cur->npages * PGSIZE;
        if (cur_end <= begin) { link = &cur->next; continue; }
        if (cur->begin >= end) break;
        if (begin <= cur->begin && end >= cur_end) {
            *link = cur->next;
            mmap_region_free(cur);
        } else if (begin <= cur->begin) {
            cur->begin = end;
            cur->npages = (uint32)((cur_end - end) / PGSIZE);
            break;
        } else if (end >= cur_end) {
            cur->npages = (uint32)((begin - cur->begin) / PGSIZE);
            link = &cur->next;
        } else {
            split->begin = end;
            split->npages = (uint32)((cur_end - end) / PGSIZE);
            split->perm = cur->perm;
            split->next = cur->next;
            cur->npages = (uint32)((begin - cur->begin) / PGSIZE);
            cur->next = split;
            break;
        }
    }
    return 0;
}

/*------------------part-3: 用户空间heap和stack管理相关------------------*/

// 用户堆空间增加, 返回新的堆顶地址 (注意栈顶最大值限制)
uint64 uvm_heap_grow(pgtbl_t pgtbl, uint64 cur_heap_top, uint32 len)
{
    const uint64 heap_base = USER_BASE + PGSIZE;
    if (pgtbl == NULL || cur_heap_top < heap_base || cur_heap_top > MMAP_BEGIN ||
        (uint64)len > MMAP_BEGIN - cur_heap_top) return 0;
    uint64 end = cur_heap_top + len;
    uint64 first = (cur_heap_top + PGSIZE - 1) & ~((uint64)PGSIZE - 1);
    uint64 limit = (end + PGSIZE - 1) & ~((uint64)PGSIZE - 1);
    struct uvm_tx tx = {pgtbl, NULL};
    for (uint64 va = first; va < limit; va += PGSIZE) {
        if (uvm_tx_map_page(&tx, va, PTE_U | PTE_R | PTE_W, NULL) < 0) {
            uvm_tx_finish(&tx, true);
            return 0;
        }
    }
    uvm_tx_finish(&tx, false);
    return end;
}

// 用户堆空间减少, 返回新的堆顶地址
uint64 uvm_heap_ungrow(pgtbl_t pgtbl, uint64 cur_heap_top, uint32 len)
{
    const uint64 heap_base = USER_BASE + PGSIZE;
    if (pgtbl == NULL || cur_heap_top < heap_base || cur_heap_top > MMAP_BEGIN ||
        (uint64)len > cur_heap_top - heap_base) return 0;
    uint64 end = cur_heap_top - len;
    uint64 first = (end + PGSIZE - 1) & ~((uint64)PGSIZE - 1);
    uint64 limit = (cur_heap_top + PGSIZE - 1) & ~((uint64)PGSIZE - 1);
    for (uint64 va = first; va < limit; va += PGSIZE) {
        pte_t *slot = vm_getpte(pgtbl, va, false);
        if (slot == NULL || (*slot & (PTE_V | PTE_U)) != (PTE_V | PTE_U) ||
            PTE_CHECK(*slot)) return 0;
    }
    if (limit > first) vm_unmappages(pgtbl, first, limit - first, true);
    return end;
}

// 处理函数栈增长导致的page fault事件
// 成功返回new_ustack_npage，失败返回-1
int64 uvm_ustack_grow(pgtbl_t pgtbl, uint64 old_ustack_npage, uint64 fault_addr)
{
    const uint64 max_pages = (TRAPFRAME - MMAP_END) / PGSIZE;
    if (pgtbl == NULL || old_ustack_npage == 0 || old_ustack_npage > max_pages ||
        fault_addr < MMAP_END || fault_addr >= TRAPFRAME) return -1;
    uint64 previous = TRAPFRAME - old_ustack_npage * PGSIZE;
    uint64 first = fault_addr & ~((uint64)PGSIZE - 1);
    if (first >= previous) return -1;
    struct uvm_tx tx = {pgtbl, NULL};
    for (uint64 va = first; va < previous; va += PGSIZE) {
        if (uvm_tx_map_page(&tx, va, PTE_U | PTE_R | PTE_W, NULL) < 0) {
            uvm_tx_finish(&tx, true);
            return -1;
        }
    }
    uvm_tx_finish(&tx, false);
    return (int64)((TRAPFRAME - first) / PGSIZE);
}

/*----------------------part-4: 用户页表管理相关----------------------*/

// 递归释放 页表占用的物理页 和 页表管理的物理页
// ps: 顶级页表level = 3
static void destroy_pgtbl(pgtbl_t pgtbl, uint32 level)
{
    assert(pgtbl != NULL && level > 0 && level <= 3,
           "destroy_pgtbl: invalid page table");
    for (uint32 i = 0; i < PGSIZE / sizeof(pte_t); i++) {
        pte_t pte = pgtbl[i];
        if (!(pte & PTE_V))
            continue;
        if (PTE_CHECK(pte)) {
            assert(level > 1, "destroy_pgtbl: invalid non-leaf");
            destroy_pgtbl((pgtbl_t)PTE_TO_PA(pte), level - 1);
        } else {
            assert(level == 1, "destroy_pgtbl: unexpected large page");
            // 用户数据页归页表所有；trapframe 与 trampoline 为借用。
            if (pte & PTE_U)
                pmem_free(PTE_TO_PA(pte), false);
        }
        pgtbl[i] = 0;
    }
    pmem_free((uint64)pgtbl, true);
}

// 页表销毁
void uvm_destroy_pgtbl(pgtbl_t pgtbl)
{
    if (pgtbl != NULL)
        destroy_pgtbl(pgtbl, 3);
}

static int uvm_copy_range(struct uvm_tx *tx, pgtbl_t old, uint64 begin, uint64 end)
{
    for (uint64 va = begin; va < end; va += PGSIZE) {
        pte_t *slot = vm_getpte(old, va, false);
        if (slot == NULL || (*slot & (PTE_V | PTE_U)) != (PTE_V | PTE_U) ||
            PTE_CHECK(*slot)) return -1;
        int perm = (int)(*slot & (PTE_R | PTE_W | PTE_X | PTE_U));
        if (!uvm_valid_perm(perm) ||
            uvm_tx_map_page(tx, va, perm, (const void *)PTE_TO_PA(*slot)) < 0)
            return -1;
    }
    return 0;
}

// 拷贝用户数据页，不复制借用的 trapframe 和 trampoline。
int uvm_copy_pgtbl(pgtbl_t old, pgtbl_t new, uint64 heap_top,
                   uint64 ustack_npage, mmap_region_t *mmap)
{
    const uint64 heap_base = USER_BASE + PGSIZE;
    const uint64 max_stack = (TRAPFRAME - MMAP_END) / PGSIZE;
    if (old == NULL || new == NULL || old == new || heap_top < heap_base ||
        heap_top > MMAP_BEGIN || ustack_npage == 0 || ustack_npage > max_stack)
        return -1;

    // 有序链中的区间须在映射区范围内，且不能溢出或交叠。
    uint64 previous_end = MMAP_BEGIN;
    unsigned regions = 0;
    for (mmap_region_t *region = mmap; region != NULL; region = region->next) {
        if (++regions > N_MMAP || region->npages == 0 ||
            (region->begin & (PGSIZE - 1)) || region->begin < previous_end ||
            region->begin >= MMAP_END || !uvm_valid_perm(region->perm | PTE_U) ||
            (uint64)region->npages > (MMAP_END - region->begin) / PGSIZE)
            return -1;
        previous_end = region->begin + (uint64)region->npages * PGSIZE;
    }

    struct uvm_tx tx = {new, NULL};
    uint64 heap_end = (heap_top + PGSIZE - 1) & ~((uint64)PGSIZE - 1);
    if (uvm_copy_range(&tx, old, USER_BASE, heap_end) < 0)
        goto failed;
    for (mmap_region_t *region = mmap; region != NULL; region = region->next) {
        uint64 end = region->begin + (uint64)region->npages * PGSIZE;
        if (uvm_copy_range(&tx, old, region->begin, end) < 0)
            goto failed;
    }
    if (uvm_copy_range(&tx, old, TRAPFRAME - ustack_npage * PGSIZE, TRAPFRAME) < 0)
        goto failed;
    uvm_tx_finish(&tx, false);
    return 0;
failed:
    uvm_tx_finish(&tx, true);
    return -1;
}

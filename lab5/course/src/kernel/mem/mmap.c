#include "mod.h"

// mmap_region_node_t 仓库(单向链表) + 链表头节点(不可分配) + 保护仓库的自旋锁
static mmap_region_node_t node_list[N_MMAP];
static mmap_region_node_t list_head;
static spinlock_t list_lk;
static bool node_used[N_MMAP];

// 初始化上述三个数据结构
void mmap_init()
{
    spinlock_init(&list_lk, "mmap nodes");
    list_head.next = NULL;
    for (int i = N_MMAP - 1; i >= 0; --i) {
        node_used[i] = false;
        memset(&node_list[i].mmap, 0, sizeof(node_list[i].mmap));
        node_list[i].next = list_head.next;
        list_head.next = &node_list[i];
    }
}

// 从仓库申请一个 mmap_region_t
// 若仓库空了则 panic
mmap_region_t *mmap_region_alloc()
{
    spinlock_acquire(&list_lk);
    mmap_region_node_t *node = list_head.next;
    if (node) {
        list_head.next = node->next;
        node_used[node - node_list] = true;
        memset(&node->mmap, 0, sizeof(node->mmap));
    }
    spinlock_release(&list_lk);
    return node ? &node->mmap : NULL;
}

// 向仓库归还一个 mmap_region_t
void mmap_region_free(mmap_region_t *mmap)
{
    // mmap 是包装结构的第一个字段；先验证地址再计算索引。
    uint64 p = (uint64)mmap;
    uint64 start = (uint64)&node_list[0];
    uint64 end = (uint64)&node_list[N_MMAP];
    assert(p >= start && p < end && (p - start) % sizeof(node_list[0]) == 0,
           "mmap_region_free: invalid node");
    uint32 index = (p - start) / sizeof(node_list[0]);
    spinlock_acquire(&list_lk);
    if (!node_used[index]) {
        spinlock_release(&list_lk);
        panic("mmap_region_free: duplicate");
    }
    node_used[index] = false;
    memset(&node_list[index].mmap, 0, sizeof(node_list[index].mmap));
    node_list[index].next = list_head.next;
    list_head.next = &node_list[index];
    spinlock_release(&list_lk);
}

// 输出可用的 mmap_region_node_t 链
// for debug
void mmap_show_nodelist()
{
    spinlock_acquire(&list_lk);

    mmap_region_node_t *tmp = list_head.next;
    int node = 0, index = 0;
    while (tmp)
    {
        index = tmp - &(node_list[0]);
        printf("node %d index = %d\n", node++, index);
        tmp = tmp->next;
    }

    spinlock_release(&list_lk);
}

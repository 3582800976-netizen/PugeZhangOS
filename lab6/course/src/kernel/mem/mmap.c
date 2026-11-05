#include "mod.h"

// mmap_region_node_t 仓库(单向链表) + 链表头节点(不可分配) + 保护仓库的自旋锁
static mmap_region_node_t node_list[N_MMAP];
static mmap_region_node_t list_head;
static spinlock_t list_lk;
// 只记录仓库节点的分配状态；与空闲链表一起受 list_lk 保护。
static uint8 node_in_use[N_MMAP];

// 初始化上述三个数据结构
void mmap_init()
{
    spinlock_init(&list_lk, "mmap nodes");
    list_head.next = NULL;
    for (int i = 0; i < N_MMAP; i++) {
        node_in_use[i] = 0;
        node_list[i].mmap.begin = 0;
        node_list[i].mmap.npages = 0;
        node_list[i].mmap.perm = 0;
        node_list[i].mmap.next = NULL;
        node_list[i].next = list_head.next;
        list_head.next = &node_list[i];
    }
}

// 从仓库申请一个 mmap_region_t
// 仓库耗尽属于可恢复的资源不足，返回 NULL。
mmap_region_t *mmap_region_alloc()
{
    spinlock_acquire(&list_lk);
    mmap_region_node_t *node = list_head.next;
    if (node == NULL) {
        spinlock_release(&list_lk);
        return NULL;
    }
    list_head.next = node->next;
    node->next = NULL;
    int index = (int)(node - node_list);
    node_in_use[index] = 1;
    node->mmap.begin = 0;
    node->mmap.npages = 0;
    node->mmap.perm = 0;
    node->mmap.next = NULL;
    spinlock_release(&list_lk);
    return &node->mmap;
}

// 向仓库归还一个 mmap_region_t
void mmap_region_free(mmap_region_t *mmap)
{
    // mmap 是包装结构的首字段。先按整数验证范围与步长，再计算索引，
    // 不对来自其他对象的指针进行未定义的指针减法。
    uint64 addr = (uint64)mmap;
    uint64 base = (uint64)&node_list[0];
    uint64 end = (uint64)&node_list[N_MMAP];
    if (addr < base || addr >= end || (addr - base) % sizeof(node_list[0]) != 0) {
        panic("mmap_region_free: invalid node");
        return;
    }
    int index = (int)((addr - base) / sizeof(node_list[0]));
    spinlock_acquire(&list_lk);
    if (!node_in_use[index]) {
        spinlock_release(&list_lk);
        panic("mmap_region_free: node not allocated");
        return;
    }
    node_in_use[index] = 0;
    node_list[index].mmap.begin = 0;
    node_list[index].mmap.npages = 0;
    node_list[index].mmap.perm = 0;
    node_list[index].mmap.next = NULL;
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

#ifndef PUGE_USER_HEAP_H
#define PUGE_USER_HEAP_H
#include "types.h"
#include "syscall.h"
#define N_MMAP 32

typedef struct mmap_region {
    uint64 begin, npages;
    struct mmap_region *next;
} mmap_region_t;
typedef struct mmap_region_node {
    mmap_region_t mmap;
    struct mmap_region_node *next;
} mmap_region_node_t;
struct process;
struct user_heap_snapshot {
    struct user_heap_info info;
    struct { uint64 begin, npages; } regions[N_MMAP];
};
void user_heap_pool_init(void);
int user_heap_create(struct process *p);
int user_heap_alloc(struct process *p, uint64 hint, uint64 npages, uint64 *out, int budget);
int user_heap_free(struct process *p, uint64 begin, uint64 npages);
int user_heap_snapshot(const struct process *p, struct user_heap_snapshot *out);
uint64 user_heap_nodes_free(void);
/* Metadata only; VM destruction subsequently releases owned pages. */
void user_heap_destroy(struct process *p);
int user_heap_selftest(void);
#endif

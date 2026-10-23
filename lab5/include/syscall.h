#ifndef PUGE_SYSCALL_H
#define PUGE_SYSCALL_H
#define SYS_GETPID 1
#define SYS_WRITE 2
#define SYS_REPORT 3
#define SYS_EXIT 4
/* Counts timer interrupts that actually arrived while this process was in U. */
#define SYS_TICKS 5
#define SYS_HEAP_ALLOC 6
#define SYS_HEAP_FREE 7
#define SYS_HEAP_INFO 8
#define SYS_HEAP_DUMP 9
#define SYS_ENOMEM (-12)
#define SYS_EFAULT (-14)
#define SYS_EINVAL (-22)
#define SYS_ENOSYS (-38)
#define SYS_WRITE_MAX 128
#ifndef __ASSEMBLER__
#include "user_frame.h"
struct user_heap_info {
    uint64 base, limit, total_pages, free_pages, allocated_pages;
    uint64 regions, largest_free_pages, node_free;
};
void syscall_dispatch(struct user_frame *frame);
struct process;
int64 syscall_memory(struct process *p, uint64 number, uint64 arg0, uint64 arg1);
#endif
#endif

#ifndef PUGE_USER_API_H
#define PUGE_USER_API_H
#include "types.h"
#include "syscall.h"
long user_syscall3(long number, uint64 x, uint64 y, uint64 z);
static inline long user_alloc_pages(uint64 pages, uint64 hint)
{
    return user_syscall3(SYS_HEAP_ALLOC, pages, hint, 0);
}
static inline long user_free_pages(uint64 address, uint64 pages)
{
    return user_syscall3(SYS_HEAP_FREE, address, pages, 0);
}
static inline long user_heap_info(struct user_heap_info *out)
{
    return user_syscall3(SYS_HEAP_INFO, (uint64)out, sizeof *out, 0);
}
static inline long user_heap_dump(uint64 tag)
{
    return user_syscall3(SYS_HEAP_DUMP, tag, 0, 0);
}
#endif

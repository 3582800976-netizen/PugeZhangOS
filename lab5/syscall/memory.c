/* This layer validates the ABI; heap and VM modules own the mechanism. */
#include "syscall.h"
#include "process.h"
#include "user_heap.h"
#include "user_vm.h"
#include "console.h"

int64 syscall_memory(struct process *p, uint64 number, uint64 arg0, uint64 arg1)
{
    if (number == SYS_HEAP_ALLOC) {
        uint64 address = 0;
        int error = user_heap_alloc(p, arg1, arg0, &address, -1);
        return error ? error : (int64)address;
    }
    if (number == SYS_HEAP_FREE)
        return user_heap_free(p, arg0, arg1);
    if (number == SYS_HEAP_INFO && arg1 != sizeof(struct user_heap_info))
        return SYS_EINVAL;
    if (number != SYS_HEAP_INFO && number != SYS_HEAP_DUMP)
        return SYS_ENOSYS;

    struct user_heap_snapshot snapshot;
    if (user_heap_snapshot(p, &snapshot)) return SYS_EINVAL;
    if (number == SYS_HEAP_INFO)
        return user_vm_copyout(&p->vm, arg0, &snapshot.info, sizeof snapshot.info) == VM_OK
                   ? 0 : SYS_EFAULT;

    const struct user_heap_info *i = &snapshot.info;
    console_printf("[heap-map] tag=%lu base=0x%lx limit=0x%lx total=%lu free=%lu used=%lu regions=%lu largest=%lu nodes_free=%lu\n",
                   arg0, i->base, i->limit, i->total_pages, i->free_pages,
                   i->allocated_pages, i->regions, i->largest_free_pages, i->node_free);
    for (uint64 n = 0; n < i->regions; n++)
        console_printf("[heap-free] tag=%lu index=%lu begin=0x%lx pages=%lu\n",
                       arg0, n, snapshot.regions[n].begin, snapshot.regions[n].npages);
    return 0;
}

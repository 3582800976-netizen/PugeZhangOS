#include "mod.h"

uint64 sys_helloworld(void)
{
    printf("proczero: hello world\n");
    return 0;
}

// 本实验的前三个调用仅用于观察内核与用户地址空间的复制。
uint64 sys_copyin(void)
{
    uint64 address, count;
    arg_uint64(0, &address);
    arg_uint64(1, &count);
    if (count > 0xffffffffULL / sizeof(int) ||
        address >= TRAPFRAME || count * sizeof(int) > TRAPFRAME - address)
        return (uint64)-1;
    int buf[16];
    uint64 done = 0;
    while (done < count) {
        uint32 n = (count - done > 16) ? 16 : (uint32)(count - done);
        if (uvm_copyin(myproc()->pgtbl, (uint64)buf,
                       address + done * sizeof(int), n * sizeof(int)) < 0)
            return (uint64)-1;
        for (uint32 i = 0; i < n; i++) printf("%d ", buf[i]);
        done += n;
    }
    printf("\n");
    return 0;
}

uint64 sys_copyout(void)
{
    static const int values[] = {1, 2, 3, 4, 5};
    uint64 address;
    arg_uint64(0, &address);
    if (uvm_copyout(myproc()->pgtbl, address, (uint64)values, sizeof(values)) < 0)
        return (uint64)-1;
    return sizeof(values) / sizeof(values[0]);
}

uint64 sys_copyinstr(void)
{
    char value[STR_MAXLEN + 1];
    if (arg_str(0, value, sizeof(value)) < 0) return (uint64)-1;
    printf("%s\n", value);
    return 0;
}

uint64 sys_printf(void)
{
    char value[STR_MAXLEN + 1];
    if (arg_str(0, value, sizeof(value)) < 0) return (uint64)-1;
    printf("%s", value);
    return 0;
}

uint64 sys_brk(void)
{
    proc_t *p = myproc();
    uint64 requested;
    arg_uint64(0, &requested);
    if (requested == 0 || requested == p->heap_top) return p->heap_top;
    if (requested < USER_BASE + PGSIZE || requested > MMAP_BEGIN) return (uint64)-1;
    uint64 delta = requested > p->heap_top ? requested - p->heap_top : p->heap_top - requested;
    if (delta > 0xffffffffULL) return (uint64)-1;
    uint64 result = requested > p->heap_top
        ? uvm_heap_grow(p->pgtbl, p->heap_top, (uint32)delta)
        : uvm_heap_ungrow(p->pgtbl, p->heap_top, (uint32)delta);
    if (result != requested) return (uint64)-1;
    p->heap_top = result;
    return result;
}

uint64 sys_mmap(void)
{
    uint64 begin, len;
    arg_uint64(0, &begin);
    arg_uint64(1, &len);
    if (!len || len > 0xffffffffULL || len % PGSIZE) return 0;
    return uvm_mmap(begin, (uint32)(len / PGSIZE), PTE_U | PTE_R | PTE_W);
}

uint64 sys_munmap(void)
{
    uint64 begin, len;
    arg_uint64(0, &begin);
    arg_uint64(1, &len);
    if (!len || len > 0xffffffffULL || len % PGSIZE) return (uint64)-1;
    return (uint64)(int64)uvm_munmap(begin, (uint32)(len / PGSIZE));
}

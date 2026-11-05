#include "mod.h"
#include "../proc/method.h"
#include "../trap/method.h"

/*
    用户堆空间伸缩
    uint64 new_heap_top (如果是0, 代表查询当前堆顶位置)
    成功返回new_heap_top, 失败返回-1
*/
uint64 sys_brk()
{
    proc_t *p = myproc();
    uint64 requested;
    arg_uint64(0, &requested);
    if (requested == 0 || requested == p->heap_top)
        return p->heap_top;
    if (requested < USER_BASE + PGSIZE || requested > MMAP_BEGIN)
        return (uint64)-1;
    uint64 delta = requested > p->heap_top ? requested - p->heap_top : p->heap_top - requested;
    if (delta > 0xffffffffULL)
        return (uint64)-1;
    uint64 result = requested > p->heap_top
        ? uvm_heap_grow(p->pgtbl, p->heap_top, (uint32)delta)
        : uvm_heap_ungrow(p->pgtbl, p->heap_top, (uint32)delta);
    if (result != requested)
        return (uint64)-1;
    p->heap_top = result;
    return result;
}

/*
    增加一段内存映射
    uint64 start 起始地址
    uint32 len   范围 (字节,需检查是否是page-aligned)
    成功返回映射空间的起始地址, 失败返回0
*/
uint64 sys_mmap()
{
    uint64 begin, len;
    arg_uint64(0, &begin);
    arg_uint64(1, &len);
    if (len == 0 || len > 0xffffffffULL || len % PGSIZE != 0)
        return 0;
    return uvm_mmap(begin, (uint32)(len / PGSIZE), PTE_U | PTE_R | PTE_W);
}

/*
    解除一段内存映射
    uint64 start 起始地址
    uint32 len   范围 (字节, 需检查是否是page-aligned)
    成功返回0 失败返回-1
*/
uint64 sys_munmap()
{
    uint64 begin, len;
    arg_uint64(0, &begin);
    arg_uint64(1, &len);
    if (len == 0 || len > 0xffffffffULL || len % PGSIZE != 0)
        return (uint64)-1;
    return (uint64)(int64)uvm_munmap(begin, (uint32)(len / PGSIZE));
}

/*
    打印一个字符串
    char *str
    成功返回0
*/
uint64 sys_print_str()
{
    char buffer[STR_MAXLEN + 1];
    if (arg_str(0, buffer, sizeof(buffer)) < 0)
        return (uint64)-1;
    printf("%s", buffer);
    return 0;
}

/*
    打印一个32位整数
    int num
    成功返回0
*/
uint64 sys_print_int()
{
    uint32 value;
    arg_uint32(0, &value);
    printf("%d", (int32)value);
    return 0;
}

/*
    进程复制
    返回子进程的pid
*/
uint64 sys_fork()
{
    return (uint64)(int64)proc_fork();
}

/*
    等待子进程退出
    uint64 addr_exit_state
*/
uint64 sys_wait()
{
    uint64 address;
    arg_uint64(0, &address);
    return (uint64)(int64)proc_wait(address);
}

/*
    进程退出
    int exit_code
    不返回
*/
uint64 sys_exit()
{
    uint32 status;
    arg_uint32(0, &status);
    proc_exit((int32)status);
    panic("sys_exit: returned");
    return (uint64)-1;
}

/*
    让进程睡眠一段时间
    uint32 ntick (1个tick大约0.1秒)
    成功返回0
*/
uint64 sys_sleep()
{
    uint64 ticks;
    arg_uint64(0, &ticks);
    if (ticks > 0xffffffffULL)
        return (uint64)-1;
    timer_wait(ticks);
    return 0;
}

/*
    返回当前进程的pid
*/
uint64 sys_getpid()
{
    proc_t *p = myproc();
    assert(p != NULL, "sys_getpid: no current process");
    return (uint64)p->pid;
}

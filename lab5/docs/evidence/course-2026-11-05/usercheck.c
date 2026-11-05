#include "sys.h"
#define PGSIZE 4096UL
#define VA_MAX (1UL << 38)
#define TRAPFRAME (VA_MAX - 2 * PGSIZE)
#define MMAP_END (TRAPFRAME - 16 * 256 * PGSIZE)
#define MMAP_BEGIN (MMAP_END - 64 * 256 * PGSIZE)
static void fail(const char *msg) { syscall(SYS_printf, msg); for (;;) {} }
__attribute__((section(".text.entry"))) int main(void)
{
    int values[5] = {0, 0, 0, 0, 0};
    if (syscall(SYS_copyout, values) != 5) fail("[lab5] copyout FAIL\n");
    for (int i=0; i<5; i++) if (values[i] != i+1) fail("[lab5] copyout content FAIL\n");
    if (syscall(SYS_copyin, values, 5) != 0) fail("[lab5] copyin FAIL\n");
    if (syscall(SYS_copyinstr, "copy-string-ok") != 0) fail("[lab5] copyinstr FAIL\n");
    if (syscall(SYS_copyin, (void *)0, 1) != -1) fail("[lab5] bad address FAIL\n");
    if (syscall(100) != -1) fail("[lab5] unknown syscall FAIL\n");
    unsigned long h = (unsigned long)syscall(SYS_brk, 0);
    if (syscall(SYS_brk, h+8192) != (long)(h+8192)) fail("[lab5] brk grow FAIL\n");
    volatile char *heap = (volatile char *)h;
    heap[0]=12; heap[4096]=34;
    if (heap[0]!=12 || heap[4096]!=34) fail("[lab5] heap access FAIL\n");
    if (syscall(SYS_brk, h) != (long)h) fail("[lab5] brk shrink FAIL\n");
    unsigned long m=(unsigned long)syscall(SYS_mmap, 0, 4*PGSIZE);
    if (m != MMAP_BEGIN) fail("[lab5] mmap FAIL\n");
    volatile char *mapped=(volatile char*)m; mapped[0]=7; mapped[3*PGSIZE]=9;
    if(mapped[0]!=7 || mapped[3*PGSIZE]!=9) fail("[lab5] mmap access FAIL\n");
    if(syscall(SYS_munmap,m+PGSIZE,2*PGSIZE)!=0) fail("[lab5] munmap split FAIL\n");
    if((unsigned long)syscall(SYS_mmap,m+PGSIZE,2*PGSIZE)!=m+PGSIZE) fail("[lab5] mmap reuse FAIL\n");
    if(syscall(SYS_munmap,m,4*PGSIZE)!=0) fail("[lab5] munmap merge FAIL\n");
    if(syscall(SYS_munmap,m,PGSIZE)!=-1) fail("[lab5] duplicate free FAIL\n");
    volatile char stack[12288]; stack[0]=77; stack[12287]=33;
    if(stack[0]!=77 || stack[12287]!=33) fail("[lab5] stack FAIL\n");
    syscall(SYS_printf,"[lab5] user copy brk mmap stack PASS\n");
    for (;;) {}
}

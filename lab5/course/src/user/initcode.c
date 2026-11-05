#include "sys.h"

int main(void)
{
    syscall(SYS_helloworld);
    syscall(SYS_helloworld);

    long heap = syscall(SYS_brk, 0);
    if (heap < 0 || syscall(SYS_brk, heap + 4096) != heap + 4096)
        goto failed;
    volatile char *h = (volatile char *)heap;
    h[0] = 42;
    long map = syscall(SYS_mmap, 0, 4096);
    if (map <= 0)
        goto failed;
    volatile char *m = (volatile char *)map;
    m[0] = 23;
    if (h[0] != 42 || m[0] != 23 || syscall(SYS_munmap, map, 4096) != 0 ||
        syscall(SYS_brk, heap) != heap)
        goto failed;
    syscall(SYS_printf, "lab5: heap and mmap OK\n");
    while (1)
        ;
failed:
    syscall(SYS_printf, "lab5: heap or mmap failed\n");
    while (1)
        ;
}

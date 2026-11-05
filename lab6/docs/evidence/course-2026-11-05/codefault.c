#include "sys.h"
int main(void){ long pid=syscall(SYS_fork); if(pid==0){__asm__ volatile("li t0, 4096; sb zero, 0(t0)" ::: "t0", "memory");syscall(SYS_exit,42);for(;;){}} int status=0;long got=syscall(SYS_wait,&status);if(pid>0 && got==pid && status==-1)syscall(SYS_print_str,"CODE-PASS\n");else syscall(SYS_print_str,"CODE-FAIL\n");for(;;){} }

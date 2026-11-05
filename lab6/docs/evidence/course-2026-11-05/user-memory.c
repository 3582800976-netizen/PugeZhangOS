#include "sys.h"
int main(void){
 long heap=syscall(SYS_brk,0);
 if(syscall(SYS_brk,heap+5000)!=heap+5000)goto bad;
 volatile char *h=(char*)heap;h[0]=23;h[4999]=42;
 if(syscall(SYS_brk,heap+100)!=heap+100 || h[0]!=23)goto bad;
 if(syscall(SYS_mmap,0,0x100001000LL)!=0)goto bad;
 long m=syscall(SYS_mmap,0,8192); if(!m)goto bad;
 volatile int *q=(int*)m;*q=0x00008067;
 if(syscall(SYS_munmap,m+4096,4096)!=0)goto bad;
 if(syscall(SYS_munmap,m+4096,4096)!=-1)goto bad;
 __asm__ volatile("li t0, 9000; sub t0, sp, t0; sb zero, 0(t0)" ::: "t0", "memory");
 long pid=syscall(SYS_fork);
 if(pid==0){((void(*)(void))m)();syscall(SYS_exit,99);for(;;){}}
 int status=0;long w=syscall(SYS_wait,&status);
 if(pid<=0||w!=pid||status!=-1)goto bad;
 if(syscall(SYS_munmap,m,4096)!=0)goto bad;
 syscall(SYS_print_str,"MEMABI-PASS\n"); for(;;){}
 bad:syscall(SYS_print_str,"MEMABI-FAIL\n");for(;;){} }

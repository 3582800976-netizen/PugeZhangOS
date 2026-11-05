#include "sys.h"
int main(void) { long invalid=syscall(0x100000007LL); long pid=syscall(SYS_fork); if(pid==0) {long x; __asm__ volatile("ld %0, 0(zero)":"=r"(x)); syscall(SYS_exit,(int)x); for(;;){}} int status=0; long got=syscall(SYS_wait,&status); if(invalid==-1 && pid>0 && got==pid && status==-1) syscall(SYS_print_str,"FAULT-PASS\n"); else syscall(SYS_print_str,"FAULT-FAIL\n"); for(;;){} }

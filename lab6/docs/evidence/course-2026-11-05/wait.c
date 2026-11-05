#include "sys.h"
int main(void) { int status=-1; long pid=syscall(SYS_fork); if(pid==0) { syscall(SYS_exit,1234); for (;;) {} } long got=syscall(SYS_wait,&status); if(pid>0 && got==pid && status==1234) syscall(SYS_print_str,"WAIT-PASS\n"); else syscall(SYS_print_str,"WAIT-FAIL\n"); for (;;) {} }

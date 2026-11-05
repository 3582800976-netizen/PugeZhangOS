#include "sys.h"
int main(void) { long pid=syscall(SYS_fork); if(pid==0) { syscall(SYS_print_str,"SLEEP-START\n"); syscall(SYS_sleep,30); syscall(SYS_print_str,"SLEEP-END\n"); syscall(SYS_exit,12); for (;;) {} } int status=-1; long got=syscall(SYS_wait,&status); if(got==pid && status==12) syscall(SYS_print_str,"SLEEP-PASS\n"); else syscall(SYS_print_str,"SLEEP-FAIL\n"); for (;;) {} }

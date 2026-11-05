#include "sys.h"
int main(void){long pid=syscall(SYS_fork); if(pid==0){syscall(SYS_print_str,"CHILD-RAN\n");syscall(SYS_exit,12);for(;;){}} syscall(SYS_print_int,42); int status=0; syscall(SYS_wait,&status); syscall(SYS_print_str,"PREEMPT-DONE\n");for(;;){} }

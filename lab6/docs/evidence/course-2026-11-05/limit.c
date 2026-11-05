#include "sys.h"
int main(void) { for(int i=0;i<40;i++){long p=syscall(SYS_fork); if(p==0){syscall(SYS_sleep,100000); syscall(SYS_exit,0);for(;;){}} if(p<0){if(syscall(SYS_getpid)==1) syscall(SYS_print_str,"LIMIT-PASS\n");else syscall(SYS_print_str,"LIMIT-FAIL\n");for(;;){}}} syscall(SYS_print_str,"LIMIT-FAIL\n");for(;;){} }

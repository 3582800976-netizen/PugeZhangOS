#include "sys.h"
int main(void) { syscall(SYS_print_str,"L1\n"); syscall(SYS_fork); syscall(SYS_print_str,"L2\n"); syscall(SYS_fork); syscall(SYS_print_str,"L3\n"); for (;;) {} }

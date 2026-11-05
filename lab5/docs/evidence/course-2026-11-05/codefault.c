#include "sys.h"
int main(void) { syscall(SYS_printf,"[lab5] write-code probe\n"); *(volatile char *)0x1000 = 0; syscall(SYS_printf,"[lab5] BAD\n"); for (;;) {} }

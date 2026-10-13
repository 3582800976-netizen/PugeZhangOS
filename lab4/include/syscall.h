#ifndef PUGE_SYSCALL_H
#define PUGE_SYSCALL_H
#define SYS_GETPID 1
#define SYS_WRITE 2
#define SYS_REPORT 3
#define SYS_EXIT 4
/* Counts timer interrupts that actually arrived while this process was in U. */
#define SYS_TICKS 5
#define SYS_EFAULT (-14)
#define SYS_EINVAL (-22)
#define SYS_ENOSYS (-38)
#define SYS_WRITE_MAX 128
#ifndef __ASSEMBLER__
#include "user_frame.h"
void syscall_dispatch(struct user_frame *frame);
#endif
#endif

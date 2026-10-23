/* Small project-local ABI. No direct user pointer dereferences in the kernel. */
#include "syscall.h"
#include "process.h"
#include "console.h"
#include "user_vm.h"

void syscall_dispatch(struct user_frame *frame)
{
    struct process *p = process_current();
    if (!p || frame != p->tf) panic("syscall without current user process");
    int64 result;
    switch (frame->x[17]) {
    case SYS_GETPID:
        result = (int64)p->pid;
        break;
    case SYS_WRITE: {
        uint64 address = frame->x[10], length = frame->x[11];
        char bytes[SYS_WRITE_MAX];
        if (length > SYS_WRITE_MAX) result = SYS_EINVAL;
        else if (user_vm_copyin(&p->vm, bytes, address, length) != VM_OK)
            result = SYS_EFAULT;
        else {
            /* Validation and copying finish before the first visible byte. */
            for (uint64 i = 0; i < length; ++i) console_putc(bytes[i]);
            result = (int64)length;
        }
        break;
    }
    case SYS_REPORT:
        p->result.report = frame->x[10];
        result = 0;
        break;
    case SYS_EXIT:
        process_finish(PROCESS_EXITED, (int64)frame->x[10]);
    case SYS_TICKS:
        result = (int64)p->result.user_timer_interrupts;
        break;
    case SYS_HEAP_ALLOC:
    case SYS_HEAP_FREE:
    case SYS_HEAP_INFO:
    case SYS_HEAP_DUMP:
        result = syscall_memory(p, frame->x[17], frame->x[10], frame->x[11]);
        break;
    default:
        result = SYS_ENOSYS;
        break;
    }
    frame->x[10] = (uint64)result;
}

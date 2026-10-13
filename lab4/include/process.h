#ifndef PUGE_PROCESS_H
#define PUGE_PROCESS_H
#include "types.h"
#include "user_frame.h"
#include "user_vm.h"
enum process_mode {
    PROCESS_NORMAL = 0, PROCESS_KERNEL_READ = 1, PROCESS_UART_READ = 2,
    PROCESS_CODE_WRITE = 3, PROCESS_STACK_EXEC = 4,
    PROCESS_FRAME_READ = 5, PROCESS_TRAMPOLINE_READ = 6, PROCESS_BAD_CALLS = 7
};
enum process_reason { PROCESS_EXITED = 0, PROCESS_FAULTED = 1, PROCESS_CREATE_FAILED = 2 };
struct process_result {
    uint64 pid, mode, report, syscalls;
    uint64 user_timer_interrupts, user_external_interrupts, user_origin;
    uint64 cause, epc, value;
    int64 exit_status;
    enum process_reason reason;
};
/* Shared only by the process/trap/call mechanisms; diagnostics get a copy. */
struct process {
    uint64 pid, pgtbl, heap_top, ustack_pages, kstack;
    struct user_frame *tf;
    struct context ctx;
    struct user_vm vm;
    struct process_result result;
    int running;
};
void process_init(void);
int process_run(unsigned mode, struct process_result *out);
struct process *process_current(void);
void process_finish(enum process_reason reason, int64 status) __attribute__((noreturn));
int process_get_result(struct process_result *out);
void user_trap(struct user_frame *frame) __attribute__((noreturn));
void trap_user_return(void) __attribute__((noreturn));
int user_process_selftest(void);
int user_memory_selftest(void);
#endif

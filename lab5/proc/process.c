/* One trusted process slot, run only by the boot leader. There is no scheduler. */
#include "process.h"
#include "platform.h"
#include "bootstate.h"
#include "page.h"
#include "vm.h"
#include "memory.h"
#include "console.h"
#include "riscv.h"
#include "syscall.h"
#include "user_layout.h"

extern char trampoline_start[];
extern char _user_code_start[], _user_code_end[];
extern char _user_data_start[], _user_data_end[];
static struct process slot;
static struct context monitor_context;
static struct process_result last_result;
static uint64 permanent_frame, permanent_stack, owner, next_pid = 1;
static int initialized, have_result;

static void first_user_return(void)
{
    trap_user_return();
}

void process_init(void)
{
    if (initialized) panic("process slot initialized twice");
    owner = boot_leader();
    user_heap_pool_init();
    permanent_frame = (uint64)page_alloc(PAGE_KERNEL);
    permanent_stack = (uint64)page_alloc(PAGE_KERNEL);
    if (!permanent_frame || !permanent_stack)
        panic("cannot reserve process frame and kernel stack");
    if (((uint64)trampoline_start & (PAGE_SIZE - 1)) ||
        vm_map_kernel(TRAMPOLINE, (uint64)trampoline_start, VM_R | VM_X) ||
        vm_map_kernel(TRAPFRAME, permanent_frame, VM_R | VM_W) ||
        vm_map_kernel(KERNEL_STACK_BASE, permanent_stack, VM_R | VM_W))
        panic("cannot map shared process slot");
    initialized = 1;
}

struct process *process_current(void)
{
    /* tp is trusted here: user entry restores it before entering any C code. */
    return initialized && r_tp() == owner && slot.running ? &slot : 0;
}

int process_get_result(struct process_result *out)
{
    if (!out || !have_result) return 0;
    *out = last_result;
    return 1;
}

int process_run(unsigned mode, struct process_result *out)
{
    struct process_result failure;
    memset(&failure, 0, sizeof failure);
    failure.mode = mode;
    failure.reason = PROCESS_CREATE_FAILED;
    if (!initialized || r_tp() != owner || slot.running ||
        mode > PROCESS_HEAP_EXEC) {
        failure.exit_status = SYS_EINVAL;
        if (out) *out = failure;
        return SYS_EINVAL;
    }
    int enabled = intr_get();
    intr_off();
    memset(&slot, 0, sizeof slot);
    slot.pid = next_pid++;
    slot.tf = (struct user_frame *)TRAPFRAME;
    slot.kstack = KERNEL_STACK_TOP;
    slot.heap_top = USER_HEAP_BASE;
    slot.ustack_pages = 1;
    slot.result.pid = slot.pid;
    slot.result.mode = mode;
    slot.result.reason = PROCESS_CREATE_FAILED;
    int error = user_vm_create(&slot.vm,
                              _user_code_start, (uint64)_user_code_end - (uint64)_user_code_start,
                              _user_data_start, (uint64)_user_data_end - (uint64)_user_data_start,
                              permanent_frame, permanent_stack, -1);
    if (!error) error = user_heap_create(&slot);
    if (error) {
        user_heap_destroy(&slot);
        user_vm_destroy(&slot.vm);
        slot.result.exit_status = error;
        last_result = slot.result;
        have_result = 1;
        if (out) *out = last_result;
        if (enabled) intr_on();
        return error;
    }
    slot.pgtbl = slot.vm.root;
    memset(slot.tf, 0, sizeof *slot.tf);
    slot.tf->epc = USER_CODE_BASE;
    slot.tf->x[2] = USER_STACK_TOP;
    slot.tf->x[10] = mode;
    slot.ctx.ra = (uint64)first_user_return;
    slot.ctx.sp = KERNEL_STACK_TOP;
    slot.running = 1;
    console_printf("[user-start] pid=%lu hart=%lu entry=0x%lx sp=0x%lx kstack=0x%lx root=0x%lx\n",
                   slot.pid, owner, slot.tf->epc, slot.tf->x[2], slot.kstack,
                   slot.pgtbl);
    /* Only integer kernel context is switched here; trap_user_return does sret. */
    context_switch(&monitor_context, &slot.ctx);

    /* EXIT/fault restored the kernel page table before switching back here. */
    intr_off();
    if (r_satp() != (SATP_SV39 | (vm_root_address() >> 12)))
        panic("process returned while user page table is active");
    last_result = slot.result;
    have_result = 1;
    user_heap_destroy(&slot);
    user_vm_destroy(&slot.vm);
    slot.pgtbl = 0;
    slot.running = 0;
    if (out) *out = last_result;
    if (enabled) intr_on();
    return 0;
}

void process_finish(enum process_reason reason, int64 status)
{
    struct process *p = process_current();
    if (!p || intr_get()) panic("invalid process finish context");
    p->result.reason = reason;
    p->result.exit_status = status;
    p->running = 0;
    context_switch(&p->ctx, &monitor_context);
    panic("finished process was resumed");
}

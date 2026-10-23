/* Execute real user programs, then check isolation and physical-page balance. */
#include "process.h"
#include "page.h"
#include "user_layout.h"
#include "console.h"

extern char _user_code_start[], _user_code_end[];
extern char _user_data_start[], _user_data_end[];

static int check(const char *name, int passed)
{
    console_printf("[utest] %s %s\n", name, passed ? "PASS" : "FAIL");
    return !passed;
}

static void free_counts(uint64 counts[2])
{
    for (int id = PAGE_KERNEL; id <= PAGE_USER; id++) {
        struct page_stats stats;
        page_get_stats(id, &stats);
        counts[id] = stats.free;
    }
}

static int counts_equal(const uint64 counts[2])
{
    uint64 now[2];
    free_counts(now);
    return now[0] == counts[0] && now[1] == counts[1];
}

static int empty_vm(const struct user_vm *vm)
{
    return !vm->root && !vm->code && !vm->data && !vm->stack;
}

static int allocation_rollback(void)
{
    int failures = 0;
    uint64 initial[2];
    free_counts(initial);
    void *frame = page_alloc(PAGE_KERNEL);
    void *stack = page_alloc(PAGE_KERNEL);
    if (!frame || !stack) {
        if (frame) page_free(PAGE_KERNEL, frame);
        if (stack) page_free(PAGE_KERNEL, stack);
        return check("rollback temporary pages", 0);
    }
    uint64 baseline[2];
    free_counts(baseline);
    for (int budget = 0; budget <= 8; budget++) {
        struct user_vm vm = {0};
        int status = user_vm_create(&vm, _user_code_start,
                                    (uint64)_user_code_end - (uint64)_user_code_start,
                                    _user_data_start,
                                    (uint64)_user_data_end - (uint64)_user_data_start,
                                    (uint64)frame, (uint64)stack, budget);
        int correct = budget < 8 ? status != 0 && empty_vm(&vm) : status == 0;
        user_vm_destroy(&vm);
        correct &= empty_vm(&vm) && counts_equal(baseline);
        console_printf("[utest] allocation-budget=%d %s\n", budget, correct ? "PASS" : "FAIL");
        failures += !correct;
    }
    int frame_status = page_free(PAGE_KERNEL, frame);
    int stack_status = page_free(PAGE_KERNEL, stack);
    failures += check("rollback borrowed pages remain allocated",
                      frame_status == PAGE_OK && stack_status == PAGE_OK);
    failures += check("rollback resource balance", counts_equal(initial));
    return failures;
}

int user_process_selftest(void)
{
    static const uint64 causes[8] = {0, 13, 13, 15, 12, 13, 13, 0};
    static const uint64 addresses[8] = {
        0, 0x80200000UL, 0x10000000UL, USER_CODE_BASE,
        USER_STACK_BASE, TRAPFRAME, TRAMPOLINE, 0
    };
    uint64 baseline[2];
    free_counts(baseline);
    int failures = allocation_rollback() + user_memory_selftest();
    for (unsigned round = 0; round < 2; round++) {
        for (unsigned mode = PROCESS_NORMAL; mode <= PROCESS_BAD_CALLS; mode++) {
            struct process_result result = {0};
            int status = process_run(mode, &result);
            int correct = status == 0 && result.pid > 0 && result.mode == mode && result.user_origin;
            if (mode == PROCESS_NORMAL) {
                correct &= result.reason == PROCESS_EXITED && result.exit_status == 0 &&
                           result.report == 5050 && result.syscalls >= 7 &&
                           result.user_timer_interrupts >= 3;
            } else if (mode == PROCESS_BAD_CALLS) {
                correct &= result.reason == PROCESS_EXITED && result.exit_status == 0 &&
                           result.syscalls >= 8;
            } else {
                correct &= result.reason == PROCESS_FAULTED && result.cause == causes[mode] &&
                           result.value == addresses[mode];
                correct &= mode == PROCESS_STACK_EXEC ? result.epc == USER_STACK_BASE :
                           result.epc >= USER_CODE_BASE && result.epc < USER_DATA_BASE;
            }
            correct &= counts_equal(baseline);
            console_printf("[utest] round=%u mode=%u reason=%u status=%ld report=%lu "
                           "origin=%lu cause=0x%lx epc=0x%lx value=0x%lx user_ticks=%lu %s\n",
                           round, mode, (unsigned)result.reason, result.exit_status, result.report,
                           result.user_origin, result.cause, result.epc, result.value,
                           result.user_timer_interrupts, correct ? "PASS" : "FAIL");
            failures += !correct;
        }
    }
    failures += check("all modes restore both page pools", counts_equal(baseline));
    console_printf("[utest] summary failures=%d\n", failures);
    return failures;
}

int user_heap_process_selftest(void)
{
    uint64 baseline[2];
    free_counts(baseline);
    uint64 nodes = user_heap_nodes_free();
    int failures = 0;
    for (unsigned round = 0; round < 2; round++) {
        for (unsigned mode = PROCESS_HEAP_DEMO; mode <= PROCESS_HEAP_EXEC; mode++) {
            struct process_result result = {0};
            int status = process_run(mode, &result);
            int correct = status == 0 && result.pid > 0 && result.mode == mode &&
                          result.user_origin;
            if (mode == PROCESS_HEAP_DEMO) {
                correct &= result.reason == PROCESS_EXITED && result.exit_status == 0 &&
                           result.report == 0x5005 && result.syscalls > 0;
            } else {
                uint64 cause = mode == PROCESS_HEAP_FREED_READ ? 13 :
                               mode == PROCESS_HEAP_FREED_WRITE ? 15 : 12;
                correct &= result.reason == PROCESS_FAULTED && result.cause == cause &&
                           result.value == USER_HEAP_BASE &&
                           result.syscalls >= (mode == PROCESS_HEAP_EXEC ? 1UL : 2UL);
                correct &= mode == PROCESS_HEAP_EXEC ? result.epc == USER_HEAP_BASE :
                           result.epc >= USER_CODE_BASE && result.epc < USER_DATA_BASE;
            }
            correct &= counts_equal(baseline) && user_heap_nodes_free() == nodes;
            console_printf("[huser] round=%u mode=%u reason=%u status=%ld report=%lu "
                           "origin=%lu cause=0x%lx epc=0x%lx value=0x%lx %s\n",
                           round, mode, (unsigned)result.reason, result.exit_status, result.report,
                           result.user_origin, result.cause, result.epc, result.value,
                           correct ? "PASS" : "FAIL");
            failures += !correct;
        }
    }
    console_printf("[huser] summary failures=%d\n", failures);
    return failures;
}

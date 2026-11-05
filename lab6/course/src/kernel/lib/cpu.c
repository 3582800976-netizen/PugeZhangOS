#include "mod.h"

static cpu_t cpus[NCPU];

cpu_t *mycpu(void)
{
    return &cpus[mycpuid()];
}

int mycpuid(void)
{
    uint64 hartid = r_tp();
    assert(hartid < NCPU, "mycpuid: invalid hart");
    return (int)hartid;
}
proc_t *myproc(void)
{
    // 获取当前进程期间禁止抢占，确保 hart 和进程指针对应。
    push_off();
    proc_t *p = mycpu()->proc;
    pop_off();
    return p;
}

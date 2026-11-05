#include "mod.h"

static cpu_t cpus[NCPU];

cpu_t *mycpu(void)
{
    uint64 hartid = r_tp();
    assert(hartid < NCPU, "mycpu: invalid hart");
    return &cpus[hartid];
}

int mycpuid(void)
{
    uint64 hartid = r_tp();
    assert(hartid < NCPU, "mycpuid: invalid hart");
    return (int)hartid;
}
proc_t *myproc(void)
{
    return mycpu()->proc;
}

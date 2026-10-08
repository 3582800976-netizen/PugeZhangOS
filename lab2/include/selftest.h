#ifndef PUGE_SELFTEST_H
#define PUGE_SELFTEST_H
#include "types.h"
void selftest_prepare(void);             /* sample free-page counts before SMP tests */
void selftest_hart(uint64 id, int primary); /* every hart joins the same boot test */
int selftest_parallel_result(void);                 /* primary checks all harts, then local tests */
int selftest_run(void);                  /* repeat format/page/VM tests from monitor */
#endif

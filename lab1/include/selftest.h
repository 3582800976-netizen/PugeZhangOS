/* 测试可以观察机制的结果，但不代替正常启动流程。 */
#ifndef PUGE_SELFTEST_H
#define PUGE_SELFTEST_H
#include "types.h"
void selftest_hart(uint64 id, int primary);
int selftest_run(void);
#endif

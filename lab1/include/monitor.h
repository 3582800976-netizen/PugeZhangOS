/* 监视台和诊断展示的接口；不拥有启动状态。 */
#ifndef PUGE_MONITOR_H
#define PUGE_MONITOR_H
void monitor_run(void) __attribute__((noreturn));
void diagnostics_boot(void);
#endif

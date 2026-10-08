#ifndef PUGE_MONITOR_H
#define PUGE_MONITOR_H
void diagnostics_boot(void);
void diagnostics_memory(void);
void diagnostics_vm(const char *argument);
void diagnostics_ticks(void);
void diagnostics_irq(void);
void monitor_run(void) __attribute__((noreturn));
#endif

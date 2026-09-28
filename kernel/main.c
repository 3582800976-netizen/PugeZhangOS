#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

enum {
  BOOT_TRACE_ENTRY             = 1U << 0,
  BOOT_TRACE_START             = 1U << 1,
  BOOT_TRACE_MRET_READY        = 1U << 2,
  BOOT_TRACE_MAIN              = 1U << 3,
  BOOT_TRACE_GLOBAL_INIT_BEGIN = 1U << 4,
  BOOT_TRACE_WAIT_STARTED      = 1U << 5,
  BOOT_TRACE_GLOBAL_INIT_DONE  = 1U << 6,
  BOOT_TRACE_RELEASED          = 1U << 7,
  BOOT_TRACE_LOCAL_INIT_DONE   = 1U << 8,
  BOOT_TRACE_SCHEDULER         = 1U << 9,
};

#define BOOT_TRACE_OUTPUT_SIZE 256

static volatile uint boot_trace_state[NCPU];

volatile static int started = 0;

static void
boot_trace_mark(int id, uint event)
{
  if(id >= 0 && id < NCPU)
    boot_trace_state[id] |= event;
}

void
boot_trace_entry(void)
{
  boot_trace_mark((int)r_mhartid(), BOOT_TRACE_ENTRY);
}

void
boot_trace_start(void)
{
  boot_trace_mark((int)r_mhartid(), BOOT_TRACE_START);
}

void
boot_trace_mret_ready(void)
{
  boot_trace_mark((int)r_mhartid(), BOOT_TRACE_MRET_READY);
}

static void
boot_trace_append(char *buffer, int *length, const char *text)
{
  while(*text && *length < BOOT_TRACE_OUTPUT_SIZE - 1)
    buffer[(*length)++] = *text++;
  buffer[*length] = '\0';
}

static void
boot_trace_print(int id)
{
  static const struct {
    uint event;
    const char *name;
  } events[] = {
    { BOOT_TRACE_ENTRY,             "ENTRY" },
    { BOOT_TRACE_START,             "START" },
    { BOOT_TRACE_MRET_READY,        "MRET_READY" },
    { BOOT_TRACE_MAIN,              "MAIN" },
    { BOOT_TRACE_GLOBAL_INIT_BEGIN, "GLOBAL_INIT_BEGIN" },
    { BOOT_TRACE_WAIT_STARTED,      "WAIT_STARTED" },
    { BOOT_TRACE_GLOBAL_INIT_DONE,  "GLOBAL_INIT_DONE" },
    { BOOT_TRACE_RELEASED,          "RELEASED" },
    { BOOT_TRACE_LOCAL_INIT_DONE,   "LOCAL_INIT_DONE" },
    { BOOT_TRACE_SCHEDULER,         "SCHEDULER" },
  };
  char trace[BOOT_TRACE_OUTPUT_SIZE];
  int length = 0;
  uint state = boot_trace_state[id];

  trace[0] = '\0';
  for(uint i = 0; i < sizeof(events) / sizeof(events[0]); i++){
    if(state & events[i].event){
      if(length > 0)
        boot_trace_append(trace, &length, " -> ");
      boot_trace_append(trace, &length, events[i].name);
    }
  }
  printf("[hart %d] %s\n", id, trace);
}

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  int id = cpuid();
  boot_trace_mark(id, BOOT_TRACE_MAIN);

  if(cpuid() == 0){
    boot_trace_mark(id, BOOT_TRACE_GLOBAL_INIT_BEGIN);
    consoleinit();
    printfinit();
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    procinit();      // process table
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    plicinit();      // set up interrupt controller
    plicinithart();  // ask PLIC for device interrupts
    boot_trace_mark(id, BOOT_TRACE_LOCAL_INIT_DONE);
    binit();         // buffer cache
    iinit();         // inode cache
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    boot_trace_mark(id, BOOT_TRACE_GLOBAL_INIT_DONE);
    __sync_synchronize();
    started = 1;
  } else {
    boot_trace_mark(id, BOOT_TRACE_WAIT_STARTED);
    while(started == 0)
      ;
    __sync_synchronize();
    boot_trace_mark(id, BOOT_TRACE_RELEASED);
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
    boot_trace_mark(id, BOOT_TRACE_LOCAL_INIT_DONE);
  }

  boot_trace_mark(id, BOOT_TRACE_SCHEDULER);
  boot_trace_print(id);
  scheduler();        
}

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

extern volatile int boot_trace_state[];

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  int id = cpuid();
  boot_trace_state[id] = 2; // BOOT_TRACE_MAIN is defined in start.c.

  if(cpuid() == 0){
    consoleinit();
    printfinit();
    printf("\n");
    printf("boot trace snapshot: ");
    for(int i = 0; i < NCPU; i++)
      printf("hart%d=%d ", i, boot_trace_state[i]);
    printf("(2 = MAIN)\n");
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
    binit();         // buffer cache
    iinit();         // inode cache
    fileinit();      // file table
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while(started == 0)
      ;
    __sync_synchronize();
    kvminithart();    // turn on paging
    trapinithart();   // install kernel trap vector
    plicinithart();   // ask PLIC for device interrupts
  }

  scheduler();        
}

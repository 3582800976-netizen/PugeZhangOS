/* First instruction of our kernel: bare M-mode or firmware S-mode entry. */
#include "platform.h"
    .section .text.entry
    .balign 4
    .globl _boot
_boot:
#if LAB == 1
    csrw mie, zero
    csrr a0, mhartid
    li a1, 0                    /* bare boot has no DTB argument */
#else
    csrw sie, zero
    csrci sstatus, 2
#endif
    li t0, CPUS
    bgeu a0, t0, .Lpark         /* reject hart IDs before indexing any array */
    mv tp, a0
    /* Claim and flag live in .data, so BSS clearing cannot erase them. */
    la t0, boot_claim
    li t1, 1
    amoswap.w.aq t2, t1, (t0)
    seqz a2, t2                /* third C argument: is primary? */
    beqz a2, .Lwait_bss
    la t0, _bss_start
    la t1, _bss_end
.Lzero:
    bgeu t0, t1, .Lpublish
    sd zero, 0(t0)
    addi t0, t0, 8
    j .Lzero
.Lpublish:
    la t0, boot_ready
    li t1, 1
    fence rw, w
    sw t1, 0(t0)
    j .Lstack
.Lwait_bss:
    la t0, boot_ready
.Lwait:
    lw t1, 0(t0)
    beqz t1, .Lwait
    fence r, rw
.Lstack:
    la sp, _boot_stack_top
    li t0, BOOT_STACK_SIZE
    mul t0, t0, a0
    sub sp, sp, t0
    call boot_start
.Lpark:
    wfi
    j .Lpark
    .section .data.boot,"aw",@progbits
    .balign 4
boot_claim: .word 0
boot_ready: .word 0
    .section .bss.stacks,"aw",@nobits
    .balign 4096
    .globl _boot_stack_base
_boot_stack_base:
    .space BOOT_STACK_SIZE * MAX_HARTS
    .globl _boot_stack_top
_boot_stack_top:

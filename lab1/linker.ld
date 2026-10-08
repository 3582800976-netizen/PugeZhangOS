/* Lab 1 裸机内核位于 0x80000000；_boot 是第一条指令。 */
OUTPUT_ARCH(riscv)
ENTRY(_boot)
SECTIONS
{
    . = 0x80000000;
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    }
    . = ALIGN(4096);
    .rodata : { *(.rodata .rodata.* .srodata .srodata.*) }
    . = ALIGN(4096);
    .data : { *(.data.boot) *(.data .data.* .sdata .sdata.*) }
    . = ALIGN(8);
    _bss_start = .;
    .bss (NOLOAD) : { *(.bss .bss.* .sbss .sbss.*) *(COMMON) }
    . = ALIGN(8);
    _bss_end = .;
    . = ALIGN(4096);
    _kernel_end = .;
    ASSERT(_kernel_end < 0x88000000, "kernel exceeds QEMU RAM")
    /DISCARD/ : { *(.eh_frame .eh_frame.*) *(.comment) }
}

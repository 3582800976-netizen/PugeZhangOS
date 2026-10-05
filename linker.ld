/* Makefile supplies KERNEL_BASE for bare Lab1 or OpenSBI Lab2+3. */
OUTPUT_ARCH(riscv)
ENTRY(_boot)
SECTIONS
{
    . = KERNEL_BASE;
    _text_start = .;
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    }
    . = ALIGN(4096);
    _text_end = .;
    _rodata_start = .;
    .rodata : { *(.rodata .rodata.* .srodata .srodata.*) }
    . = ALIGN(4096);
    _rodata_end = .;
    _data_start = .;
    .data : { *(.data.boot) *(.data .data.* .sdata .sdata.*) }
    _data_end = .;
    . = ALIGN(8);
    _bss_start = .;
    .bss (NOLOAD) : { *(.bss .bss.* .sbss .sbss.*) *(COMMON) }
    . = ALIGN(8);
    _bss_end = .;
    . = ALIGN(4096);
    _kernel_end = .;
    ASSERT(_kernel_end < 0x82200000, "kernel overlaps QEMU5.1 firmware DTB")
    /DISCARD/ : { *(.eh_frame .eh_frame.*) *(.comment) }
}

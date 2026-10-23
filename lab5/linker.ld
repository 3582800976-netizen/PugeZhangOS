/* Lab 4 follows OpenSBI; the shared trampoline occupies one RX page. */
OUTPUT_ARCH(riscv)
ENTRY(_boot)
SECTIONS
{
    . = 0x80200000;
    _text_start = .;
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
    }
    . = ALIGN(4096);
    .trampoline : { KEEP(*(.trampoline)) }
    ASSERT(SIZEOF(.trampoline) <= 4096, "trampoline must fit one page")
    ASSERT((ADDR(.trampoline) & 4095) == 0, "trampoline must be page aligned")
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

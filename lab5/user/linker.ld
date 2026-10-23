OUTPUT_ARCH(riscv)
ENTRY(_user_start)
SECTIONS
{
    . = 0x1000;
    .text : {
        KEEP(*(.text.entry))
        *(.text .text.*)
        *(.rodata .rodata.* .srodata .srodata.*)
    }
    ASSERT(. <= 0x2000, "user code and constants exceed one page")
    . = 0x2000;
    .data : {
        *(.data .data.* .sdata .sdata.*)
        . = ALIGN(8);
        _user_bss_start = .;
        *(.bss .bss.* .sbss .sbss.*) *(COMMON)
        _user_bss_end = .;
    }
    PROVIDE(__global_pointer$ = 0x2800);
    ASSERT(. <= 0x3000, "user data and BSS exceed one page")
    /DISCARD/ : { *(.eh_frame .eh_frame.*) *(.comment) }
}

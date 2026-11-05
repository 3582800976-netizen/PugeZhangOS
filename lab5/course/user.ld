OUTPUT_ARCH( "riscv" )
ENTRY( main )

SECTIONS
{
  /* 用户程序从USER_BASE = 0x1000开始, 它下面的一页不建立任何映射 */
  . = 0x1000;

  .text : {
    *(.text.entry)
    *(.text.main)
    *(.text .text.*)
  }

  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*)
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data ALIGN(16) : {
    *(.sdata .sdata.*)
    *(.data .data.*)
  }

  .bss ALIGN(16) : {
    *(.sbss .sbss.*)
    *(.bss .bss.*)
    *(COMMON)
  }
}

/* 内核直接把initcode搬运到USER_BASE并从那里开始执行 */
ASSERT(main == 0x1000, "error: main is not located at 0x1000")
/* 内核只为initcode映射一个物理页 */
ASSERT(. <= 0x1000 + 0x1000, "error: initcode is larger than one page")
/* 与课程 RXU 代码页权限一致；可写数据应使用用户栈或堆。 */
ASSERT(SIZEOF(.data) == 0 && SIZEOF(.bss) == 0, "error: user image contains writable static data")

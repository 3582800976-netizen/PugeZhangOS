# Lab 5 课程框架检查：2026-11-05

本次使用 `lab5/course/` 的老师框架，衔接用户进程及地址空间，并实现系统调用、用户数据复制、`brk`、`mmap`/`munmap` 和用户栈按需增长。这里记录的是具体测试结果，不等于课程全部验收。

在仓库根目录可以复现：

```bash
make -C lab5/course build
python3 lab5/docs/evidence/course-2026-11-05/run-qemu-checks.py
(cd lab5/course && sh tests/run-host.sh)
python3 scripts/check-course-user-layout.py
python3 lab6/course/tests/printf_float/run.py --course lab5/course
```

- [QEMU 汇总](summary.txt)：两核启动、用户堆与映射演示、跨页用户数据复制、`brk`、映射拆分和重用、栈增长等测试通过；写用户代码页产生异常后内核仍在运行。具体输出见 `baseline-runtime.log`、`usercheck-runtime.log`、`codefault-runtime.log`。脚本在临时副本中替换测试程序，收集标记后停止 QEMU。
- [宿主检查](host-tests.log)：物理页、内核页表、用户复制和映射区操作等通过。宿主测试使用模拟的内存或锁，不代表硬件 MMU 测试。
- [浮点打印](float-tests.log)：3000 个有限 binary64 值与整数参考结果比较；RISC-V QEMU 测试覆盖格式和浮点寄存器保留。6 位小数按课程要求截断。
- [GDB 记录](gdb.log)：QEMU 在内核 `_entry` 的 `0x80200000` 处命中断点。
- [用户布局检查](user-layout.log)：当前用户映像仅提供一页 RXU 代码；链接脚本拒绝可写全局变量，用户程序需使用栈或堆。用户 ELF 可能报告 RWX 段链接警告，运行时页权限由页表设置。

测试覆盖有限，特别是尚未在 VisionFive 2 实板验证，也未完成所有课程规格文字和全面压力测试。内核与测试有 Agent 参与。

课程 `console.yaml` 还列出 `printfloat(double, int)` 的具名接口；当前实现是内部 `printfloat_bits`，已经测试的是 `printf` 的 `%f` 六位输出，尚未验证或提供那个独立具名接口。

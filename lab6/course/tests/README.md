# Lab 6 测试入口

在 `lab6/course/` 执行 `sh tests/run-host.sh`，运行睡眠锁、物理页、页表和用户内存操作等宿主检查；这些检查使用模拟对象，不代替内核运行。

`python3 tests/printf_float/run.py` 比较浮点格式化输出，并使用 RISC-V QEMU 检查浮点寄存器保持。

在仓库根目录执行 `python3 scripts/check-course-memmove.py --labs 6`，直接检查本阶段实际的 `lib/utils.c`，覆盖重叠复制等边界。现有 UVM 宿主检查使用的工具函数替身不能代替这项检查；RISC-V 实测及修复说明见 [memmove 维护记录](../../../docs/evidence/memmove-2026-11-10/README.md)。

独立 QEMU 集成场景及实际日志见 [Lab 6 课程测试记录](../../docs/evidence/course-2026-11-05/README.md)。其中进程创建、调度、等待、睡眠和异常测试均使用临时副本，不能证明所有并发情况，也不能代替开发板测试。

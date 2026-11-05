# Lab 6 测试入口

在 `lab6/course/` 执行 `sh tests/run-host.sh`，运行睡眠锁、物理页、页表和用户内存操作等宿主检查；这些检查使用模拟对象，不代替内核运行。

`python3 tests/printf_float/run.py` 比较浮点格式化输出，并使用 RISC-V QEMU 检查浮点寄存器保持。

独立 QEMU 集成场景及实际日志见 [Lab 6 课程测试记录](../../docs/evidence/course-2026-11-05/README.md)。其中进程创建、调度、等待、睡眠和异常测试均使用临时副本，不能证明所有并发情况，也不能代替开发板测试。

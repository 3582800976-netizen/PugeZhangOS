# Lab 4 课程框架测试：2026-11-05

本阶段在 `lab4/course/` 的老师框架中衔接 Lab 3，加入用户页表、独立内核栈、首进程 context 切换、trampoline、U/S 陷入和最小系统调用。页分配失败返回 NULL，映射操作采用失败回滚，首进程初始化失败释放已获得资源；用户异常时回到内核并回收用户页、页表和 trapframe。

## 构建和运行

在仓库根目录执行：

```bash
make -C lab4/course build
make -C lab4/course run
```

[build.log](build.log) 记录了实际构建；课程用户映像把代码和数据放在同一页，链接器报告 RWX 段警告，本阶段用户页表将此页映射为 RXU。QEMU 使用两核、130 MiB、OpenSBI，启动后观察到两次 `proczero: hello world`，见 [runtime.log](runtime.log)。测试程序会继续循环；日志中的 SIGTERM 表示测试主动终止 QEMU。

`make -C lab4/course debug GDBPORT=26144` 已通过 GDB 连接，在 `_entry` 的 `0x80200000` 命中硬件断点；[gdb.log](gdb.log)、[gdb-qemu.log](gdb-qemu.log) 保留实际输出。调试端口仅监听本机；可通过 `python3 lab4/docs/evidence/course-2026-11-05/gdb-recheck.py` 复查。

## 用户态中断和错误处理

运行 `python3 lab4/docs/evidence/course-2026-11-05/run-recheck.py` 会在临时副本应用补丁、构建并运行两个测试，不改变正式源码：

- [test-only.patch](test-only.patch) 在用户态时钟处理路径增加一次标记；[test-runtime.log](test-runtime.log) 显示两次系统调用、用户态时钟中断，以及键盘输入、退格和回车的串口回显。
- [fault-only.patch](fault-only.patch) 改写临时用户程序：未知调用必须返回 -1，然后执行一次合法调用，最后读取零页。[fault-runtime.log](fault-runtime.log) 显示 cause 13，用户执行停止后内核继续响应串口。

两个临时构建的原始输出分别为 [test-build.log](test-build.log)、[fault-build.log](fault-build.log)。

运行 `python3 lab4/docs/evidence/course-2026-11-05/protection-check.py` 会分别在真实 U 模式中尝试读取 UART、读取 trapframe、写代码页、执行用户栈；两次读取触发 cause 13，写入触发 cause 15，执行触发 cause 12。日志见 [UART](uart-read.log)、[trapframe](trapframe-read.log)、[代码页](code-write.log) 和 [用户栈](stack-exec.log)，各次异常后内核继续运行。

运行 `python3 lab4/docs/evidence/course-2026-11-05/oom-check.py` 会在临时副本注入页分配失败，依次覆盖 trapframe、用户页表、页表中间页、代码页、栈页及映射构造等 10 个分配边界；每次检查失败前后内核页和用户页的空闲计数。[oom-runtime.log](oom-runtime.log) 是一次双核 QEMU 的真实结果，10 个边界的页数均恢复。测试为了观察失败回收，将临时副本的失败终止改成打印并返回，正式内核的必要启动失败仍会 panic。

以上是 QEMU 测试，不是 VisionFive 2 实测。用户寄存器的完整覆盖和长时间压力测试仍需补充；不能把这些场景写成已通过。

原实验接口保留一个用户进程，没有多进程调度。`spec/modules/process.yaml` 和 `usertrap.yaml` 的前置条件、作用与结果由 Agent 根据当前实现起草；个人理解仍需本人补充，不能把 Agent 的实现当作本人手写。

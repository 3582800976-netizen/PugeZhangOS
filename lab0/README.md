# Lab 0：准备环境，运行参考 xv6

Lab 0 的目标是确认工具能工作，并分清宿主 Linux、QEMU 和 xv6。它还不要求编写本项目的独立内核。

| 工具 | 负责什么 |
| --- | --- |
| RISC-V GCC | 把 C 和汇编编译成 RISC-V 机器能执行的程序 |
| QEMU | 模拟 CPU、内存和设备 |
| xv6 | 运行在 QEMU 模拟机器里的参考操作系统 |
| GDB | 调试程序，观察寄存器与内存 |

## 已有记录

参考 xv6 位于仓库旁的 `../xv6-labs-2020/`，其 `util` 分支已经由你亲手启动。你的 `echo hello-xv6` 和 `ls` 输出保存在 [本人实验记录](../docs/MY_LAB_NOTES.md)。

2026-10-04 的 Agent 独立构建与运行验证记录：[构建](../docs/evidence/lab0-build.log)、[运行](../docs/evidence/lab0-runtime.log)。GDB 实际连接和实体开发板运行尚未在该次验证中检查。

## 再次运行

```bash
cd /home/zpg/Work/xv6-labs-2020
make qemu
```

出现 xv6 的 `$` 后输入 `echo hello-xv6`、`ls`。这里的 `$` 属于 xv6 的 Shell。退出 QEMU：按 `Ctrl+A`，松开，再按 `X`。

本项目 Lab 1–3 提供 `quit` 命令；参考 xv6 的 Shell 没有同样的命令。

接下来阅读 [Lab 1：启动内核](../lab1/README.md)。课程参考：[ECNU Lab 0 课件](https://gitee.com/christinaaa/ecnu-oslab-2026-task/blob/master/lab-ppt/拔尖班-lab-0.pptx)。

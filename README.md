# PugeZhangOS

张璞格的 ECNU OSLab 2026 操作系统实验。以 C 和 RISC-V 为基础，参考 xv6 的架构思想，独立组织接口与实现，逐步从机器启动发展到完整内核。

当前实现 Lab 1–3：启动、串口与格式化输出、物理页分配、Sv39 内核页表、陷阱入口、串口输入中断、每核时钟中断。Lab 4–9 尚未实现。

## 直接运行

在项目目录执行：

```sh
make qemu LAB=1 CPUS=3
make qemu LAB=2 CPUS=3
make qemu LAB=3 CPUS=3
```

一次运行一个阶段。默认 `make qemu` 启动 Lab 3、三核。`CPUS` 支持 1–8，构建参数与模拟器核数必须一致。不同阶段和核数使用独立的 `build/labN-cpuM/`，并自动跟踪头文件依赖。

看到 `pgos> ` 后输入 `help`。输入 `quit` 正常退出，不必使用终端快捷键。

| 命令 | 作用 |
| --- | --- |
| `boot` | 每个核的启动阶段、栈范围、实际页表寄存器 |
| `mem` | 内核和用户页池的总页数、空闲页数、已分配页数与错误归还次数 |
| `vm` | 展示代码、常量、数据、设备及保护区域的映射权限 |
| `vm 40000000` | 展示诊断别名的三级页表索引和实际物理地址 |
| `ticks` | 查看每个核的时钟中断次数，再次执行能观察增长 |
| `irq` | 查看实际串口中断、接收字节和缓冲丢弃计数 |
| `test` | 重复执行本阶段的自检 |
| `echo 内容` | 测试输入与行编辑 |
| `quit` | 关闭 QEMU |

这是在 S-mode 内核中执行的诊断台，不是用户态 Shell。Lab 1/2 使用轮询输入；Lab 3 使用 PLIC 中断与接收环形缓冲。未到对应阶段的功能会明确说明未启用。

## 三个阶段

| 阶段 | 启动方式 | 内容 |
| --- | --- | --- |
| Lab 1 | `-bios none`，`0x80000000`，自己完成 M→S | BSS 初始化、独立栈、PMP、特权切换、串口、自旋锁和安全格式化、多核就绪发布 |
| Lab 2 | OpenSBI，`0x80200000`，S-mode 入口 | 两个物理页池、锁与归还检查、三级页表、每核启用 Sv39、权限和地址翻译验证 |
| Lab 3 | OpenSBI，沿用 Lab 2 | 保存/恢复整数寄存器、PLIC UART 接收、SBI 每核时钟、输入编辑与异常诊断 |

固件、内核栈和 DTB 不进入空闲页链表。代码 RX、只读数据 R、数据与 RAM RW；固件和 CLINT 不映射。每个核使用同一内核页表，但分别设置自己的 `satp`。

特色功能围绕让系统状态可观察：每核启动记录、页所有权检查、虚拟地址翻译查询、真实的中断计数。额外的虚拟地址 `0x40000000` 指向一个专用物理页，自检通过实际读写证明地址翻译；主动触发并恢复预期的页错误，用来检查写保护。

## 验证与理解

```sh
make check
```

运行 Lab 1–3，每阶段分别测试 1、2、3、8 核。只检查一个配置：

```sh
python3 scripts/check.py --lab 3 --cpus 3
```

实际结果和完整 UART 日志保存在 [验证目录](docs/evidence/lab123/summary.txt)。支持环境固定为 QEMU virt、128MiB 内存；本机验证工具是 QEMU 5.1.0、其默认 OpenSBI 0.7、RISC-V GCC 13.3.0。关机使用 QEMU 平台的 32 位 finisher，避开旧固件的访问宽度兼容问题。

[Lab 1–3 详细设计与易懂说明](docs/LAB123_DESIGN.md)解释每个模块、验收方式、特色和 Agent 分工。

[本人实验记录](docs/MY_LAB_NOTES.md)保留本人的真实操作和理解。根据 2026-10-05 的新授权，本轮代码和自动验证由 Agent 完成；本人此前参与了 xv6 运行、独立栈、特权切换和整数拆分的学习。本轮不把 Agent 工作记成本人手写，也不自动判定本人理解已全部通过。

## 目录地图

```text
boot/       汇编入口、启动约定、陷阱寄存器保存恢复
kernel/     多核初始化、锁、物理页、页表、中断、诊断台
lib/        格式化输出、内存操作、SBI 调用
drivers/    UART 和 PLIC 驱动
include/    类型、硬件常量与模块接口
scripts/    自动构建与真实 QEMU 验证
docs/       设计、学习记录和证据
build/      各配置的生成文件，Git 忽略
shell/      旧宿主 Linux Shell 练习，与当前内核独立
```

旁边的 `../xv6-labs-2020/` 是参考实现，`../qemu-5.1.0/` 是模拟器源码。当前内核不链接它们的代码。

## 历史与范围

`LAB1_WORKLOG.md`、`docs/LAB1_REDESIGN.md` 保存先前 xv6 原型及逐步教学记录，其描述不代表本轮完成的最新源码。旧代码和 Git 状态快照在 `../archives/`。

本轮 Lab 1–3 在本地 `lab-3` 分支一次集成，使用 `LAB` 参数选择可运行阶段；`main` 保留原历史起点。该集成提交如实记录 Agent 辅助，不是三次独立完成实验的历史记录。

尚无用户进程、系统调用、调度、磁盘或文件系统。本轮验证面向前三个实验，实体开发板适配需按后续课程要求另行完成。

参考：[ECNU OSLab 2026 课程任务](https://gitee.com/christinaaa/ecnu-oslab-2026-task)、[RISC-V 特权架构](https://docs.riscv.org/reference/isa/priv/supervisor.html)、[SBI 定时器](https://github.com/riscv-non-isa/riscv-sbi-doc/blob/master/src/ext-time.adoc)、[SBI hart 管理](https://github.com/riscv-non-isa/riscv-sbi-doc/blob/master/src/ext-hsm.adoc)。

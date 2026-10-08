# PugeZhangOS · 系统框架与实验路线

ECNU OSLab 2026：参考 xv6 的原理，用 C 和 RISC-V 从启动、内存管理到中断逐步构建内核。

**先读 [整个操作系统的架构](ARCHITECTURE.md)，再看 [Lab 0–9 建设路线](docs/ROADMAP.md)，最后进入当前实验。**

Lab 1–3 各有自己的完整源码、Makefile 和说明；Lab 4–9 已有独立的目标与接入规划，尚未实现。实验目录区分阶段，模块目录区分职责。

| 实验 | 本次解决什么问题 | 从哪里开始 |
| --- | --- | --- |
| Lab 0 | 准备工具，运行参考 xv6 | [环境说明](lab0/README.md) |
| Lab 1 | CPU 怎样进入内核，并把字符输出到终端 | [Lab 1 说明](lab1/README.md) |
| Lab 2 | 内核怎样分配物理页、建立受权限保护的页表 | [Lab 2 说明](lab2/README.md) |
| Lab 3 | 时钟和输入怎样打断程序，处理后继续执行 | [Lab 3 说明](lab3/README.md) |
| Lab 4 | 怎样运行第一个用户进程 | [Lab 4 规划](lab4/README.md) |
| Lab 5 | 用户程序怎样调用内核服务、管理用户内存 | [Lab 5 规划](lab5/README.md) |
| Lab 6 | 怎样调度进程、处理创建和结束 | [Lab 6 规划](lab6/README.md) |
| Lab 7 | 怎样读写磁盘并缓存数据块 | [Lab 7 规划](lab7/README.md) |
| Lab 8 | 怎样由路径找到 inode 和目录项 | [Lab 8 规划](lab8/README.md) |
| Lab 9 | 怎样提供文件接口和用户 Shell | [Lab 9 规划](lab9/README.md) |

## 目录一眼看清

```text
PugeZhang/
├── ARCHITECTURE.md    全系统模块、依赖关系、现有与规划边界
├── lab0/              工具与 xv6 环境说明
├── lab1/              启动、串口、格式化输出、锁
│   ├── README.md      目标、文件职责、阅读顺序、验收方法
│   ├── Makefile       只编译本实验
│   ├── linker.ld      本实验的加载地址和内存布局
│   ├── boot/          入口与栈设置
│   ├── kernel/        启动编排、多核状态与锁
│   ├── trap/          异常入口与处理
│   ├── monitor/       输入命令与状态展示
│   ├── tests/         自检编排
│   ├── drivers/       硬件访问
│   ├── lib/           打印、内存等辅助函数
│   ├── include/       本实验的类型、常量、接口
│   └── docs/evidence/ 本实验的真实验证日志
├── lab2/              同样结构；新增 mm/ 物理页、Sv39 和权限验证
├── lab3/              同样结构；增加 PLIC、串口中断和时钟
├── lab4/ … lab9/       后续阶段规划；实现时继承前一实验的完整基础
├── scripts/check.py   自动构建、驱动 QEMU、收集各实验日志
├── docs/              全系统路线、开发约定与课程依据
├── extras/host-shell/  早期宿主 Linux Shell 练习
└── Makefile           实验运行与验证的导航入口
```

Lab 2 包含它所需要的启动和打印基础；Lab 3 包含它所需要的启动和内存基础。这些文件按阶段保留，便于直接查看和比较，每个实验可以独立构建。内核源码不跨实验引用，也不使用 `#if LAB` 切换行为。

## 分别运行

在仓库根目录执行，一次启动一个实验：

```bash
make -C lab1 qemu CPUS=3
make -C lab2 qemu CPUS=3
make -C lab3 qemu CPUS=3
```

也可以 `cd lab1` 后执行 `make qemu`，Lab 2、3 同理。默认三核，`CPUS` 支持 1–8。编译产物分别存放在各实验的 `build/cpuN/`，不会混用。

进入 `pgos>` 后输入 `help` 查看**本实验**的命令，输入 `quit` 退出。这里是内核诊断台。用户进程、系统调用和文件系统属于后续实验。

根目录单独执行 `make` 会显示使用方法。旧命令 `make qemu LAB=1 CPUS=3` 仍可用：根 Makefile 只转发到 `lab1/Makefile`，不在同一套源码中切换阶段。

## 验证

```bash
make -C lab1 check          # 只检查 Lab 1 的 1、2、3、8 核
make -C lab2 check          # 只检查 Lab 2
make -C lab3 check          # 只检查 Lab 3
make check                 # 检查三个实验全部 12 个配置
```

单一配置可用 `python3 scripts/check.py --lab 2 --cpus 3`。检查包含重新编译、ELF 入口、多核启动、自检、真实串口交互和正常退出；Lab 2 加入页池与 MMU 检查，Lab 3 加入每核时钟与串口中断计数。

验证记录归各实验所有，运行汇总如下：

- [Lab 1](lab1/docs/evidence/summary.txt)
- [Lab 2](lab2/docs/evidence/summary.txt)
- [Lab 3](lab3/docs/evidence/summary.txt)


各实验的 `docs/evidence/cpuN/` 分别保存 `build.log`、`elf.log` 和 `runtime.log`；独立构建证明在同一实验的 `docs/evidence/standalone-build.log`。`make check` 的总体结果打印到终端，每个实验保存自己的汇总。

**2026-10-08 重整理后：12 个配置全部通过、0 个失败；三个实验分别复制到仓库外编译也全部通过。**

支持环境为 QEMU `virt`、128 MiB RAM；已验证的工具为 QEMU 5.1.0、默认 OpenSBI 0.7、RISC-V GCC 13.3.0。Lab 1 自己完成 M→S 切换；Lab 2、3 由 OpenSBI 提供 S 模式入口。两个启动方式的加载地址与代码分别固定在各自目录。

## 怎样理解和比较

[系统架构](ARCHITECTURE.md)解释完整模块及未来接入边界；[开发约定](docs/DEVELOPMENT.md)说明后续阶段怎样接续。源码阅读顺序、实验说明和验收现象分别写在对应 Lab 的 README 中。

小巧思也按阶段区分：Lab 1 的每核启动记录，Lab 2 的页归还检查与虚拟地址翻译窗口，Lab 3 的真实中断计数。它们用于观察内核实际状态。

[Lab 0 本人实验记录](lab0/docs/NOTES.md)保留真实的 xv6 操作与理解。后续个人记录随对应实验放在 `labN/docs/NOTES.md`，有实际记录时再创建。本轮内核实现、分目录整理和自动验证由 Agent 完成，记录归属如实保留。

过期设计和日志已移出当前项目，旧版本仍可通过 Git 历史查看。实验资料与日志归对应 Lab，根 `docs/` 仅保存全系统文档。GitHub 的 `main` 首页用于整体框架和全阶段索引，`lab-3` 同步保存本次已验收的基础版本。

参考：[ECNU 课程任务](https://gitee.com/christinaaa/ecnu-oslab-2026-task)。旁边的 `../xv6-labs-2020/` 用于参考，当前内核不链接其代码。

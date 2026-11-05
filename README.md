# PugeZhangOS：操作系统课程实验

这是 ECNU OSLab 课程项目。我们在老师提供的 RISC-V 实验框架中逐步填写内核，借鉴 xv6 的原理，并用 QEMU 检查实际行为。课程原件见 [Gitee 课程仓库](https://gitee.com/christinaaa/ecnu-oslab-2026-task)，各阶段框架来源见[课程框架记录](docs/COURSE_FRAMEWORK.md)。

**目前的状态：** Lab 1～6 的 `course/` 已填写，并有对应的 QEMU 测试记录；这不代表所有课程验收项已经完成。VisionFive 2 的平台常量仍有待填写，尚未运行我们的内核；课程要求的实验继承分支、部分规格说明和少量接口仍需整理。代码和测试有 Agent 参与，需要如实说明。

## 整体架构

```text
用户程序 src/user/                申请内存、创建进程、请求输出
       │ ecall（系统调用）
       ▼
内核 src/kernel/
  ├─ boot/       启动入口、启动栈，与固件交接
  ├─ main.c      初始化各模块，启动第一个进程和调度器
  ├─ lib/        串口、打印、CPU 等基础功能
  ├─ lock/       自旋锁和睡眠锁，协调多个执行者
  ├─ mem/        物理页、页表、用户地址空间、堆与映射
  ├─ trap/       接收异常、系统调用、时钟和串口中断
  ├─ syscall/    检查并分派来自用户程序的请求
  └─ proc/       进程、上下文切换、调度、等待与退出
       │
       ▼
OpenSBI 固件 → QEMU 模拟的 RISC-V CPU、内存、串口等硬件
```

这是一张 Lab 6 阶段的功能图：前面的实验只包含当时需要的部分。比如 Lab 1 还没有用户进程和系统调用，Lab 4 才开始运行用户程序，Lab 6 再扩展为多个进程。Lab 1 使用自己的 M 模式入口；Lab 2～6 的 QEMU 配置使用 OpenSBI。上图表示功能关系，实际启动顺序是固件先运行，再进入内核。

进程要申请内存时，路径是：用户程序执行 `ecall` → `trap/` 保存现场 → `syscall/` 检查请求 → `mem/` 分配或映射 → 恢复现场，返回用户程序。多个进程需要轮流执行时，`proc/` 的调度器负责选择下一个进程，时钟中断提供切换时机。

## 目录与实验

```text
PugeZhang/
├── README.md           项目入口和整体架构
├── docs/               课程来源、路线和说明
├── lab0/               工具、xv6 和开发板资料
├── lab1/ ... lab6/     每个阶段各自独立
│   ├── course/         老师该阶段的指导书、框架和我们填写的源码
│   │   ├── src/kernel/ 内核代码（模块随阶段增加）
│   │   ├── src/user/   用户程序（后期阶段）
│   │   └── spec/       课程规格（部分阶段）
│   ├── docs/evidence/  实际运行记录和可复现检查
│   └── README.md       本阶段说明；其中旧内容也保留作参考
├── lab7/ ... lab9/     后续阶段规划，以老师实际发布任务为准
└── scripts/            项目辅助脚本
```

| 实验 | 学什么 | 老师框架及已记录的检查 |
| --- | --- | --- |
| Lab 1 | 启动、串口输出、锁 | [框架](lab1/course/README.md) · [复测](lab1/docs/evidence/recheck-2026-11-04/) |
| Lab 2 | 物理内存与页表 | [框架](lab2/course/README.md) · [复测](lab2/docs/evidence/recheck-2026-11-04/) |
| Lab 3 | 时钟、串口中断 | [框架](lab3/course/README.md) · [复测](lab3/docs/evidence/recheck-2026-11-04/) |
| Lab 4 | 用户态、异常和系统调用 | [框架](lab4/course/README.md) · [测试记录](lab4/docs/evidence/course-2026-11-05/README.md) |
| Lab 5 | 用户内存、堆、映射和系统调用 | [框架](lab5/course/README.md) · [测试记录](lab5/docs/evidence/course-2026-11-05/README.md) |
| Lab 6 | 创建进程、调度、锁、等待和退出 | [框架](lab6/course/README.md) · [测试记录](lab6/docs/evidence/course-2026-11-05/README.md) |

**看代码时先进入对应的 `labN/course/`。** 每个 `course/` 是老师该实验分支的独立快照，运行时不会自动引用其他 Lab 的源码。此前我们自己做的 Lab 1～5 保留在 `labN/` 其他位置，用于学习对照；它们的 `pgos>` 和测试结果不等同于课程框架的结果。

## 运行老师的框架

先准备好 RISC-V 交叉编译工具和 QEMU，见 [Lab 0](lab0/README.md)。在项目根目录，例如运行 Lab 6：

```bash
cd /home/zpg/Work/PugeZhang
make -C lab6/course run
```

把 `lab6` 换成 `lab1`～`lab5` 可以运行对应实验。Lab 6 支持 `make -C lab6/course run CPUNUM=1` 或 `CPUNUM=2`。退出 QEMU：先按 `Ctrl+A`，松开后按 `X`。输出与测试入口见上表；多数测试运行后会等待，测试脚本在收集结果后主动停止 QEMU。

Lab 4 的用户页按 RXU 映射；Lab 5、6 目前将用户程序代码放在一个只读可执行页，链接脚本会拒绝需要可写全局数据的程序，演示程序使用栈和堆。Lab 5、6 链接器可能对用户 ELF 提示 RWX 段警告；实际用户页权限和链接段警告是两个不同层面，不能只凭警告判断运行时权限。完整边界与局限见各阶段测试记录。

## 后续工作和硬件

- 继续对照老师规格与 PPT 逐项验收，补充压力测试和规格解释；QEMU 的有限测试不能证明所有情况正确。
- 按课程要求整理实验继承分支；当前目录分阶段不等于已经满足分支要求。
- VisionFive 2 需要单独适配启动、内存和设备，并记录真实启动结果。此前写入 microSD 的 Ubuntu 镜像用于开发板环境，**不是我们的内核**。
- 学习时建议依次看 Lab 4 的用户态切换、Lab 5 的用户内存，再看 Lab 6 的 `fork`、调度与 `wait`；详细文件和运行记录在各阶段链接里。

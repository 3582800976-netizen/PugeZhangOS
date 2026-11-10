# PugeZhangOS：操作系统课程实验

**完整框架总文档：[项目完整框架与当前进度](docs/SYSTEM_FRAMEWORK.md)。**

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

## 每个 Lab 的框架和进度

下面的“通过”指对应记录中的具体 QEMU 场景通过，不代表课程全部验收或开发板验证通过。每个实验都是一个独立阶段；可以把它们理解为系统逐步长大的六张照片。

### Lab 1：先让内核运行起来

CPU 启动后，需要有入口、栈，以及能看见结果的输出。主要代码是 [boot/entry.S](lab1/course/src/kernel/boot/entry.S)、[boot/start.c](lab1/course/src/kernel/boot/start.c)、[lib/print.c](lab1/course/src/kernel/lib/print.c) 和 [lock/spinlock.c](lab1/course/src/kernel/lock/spinlock.c)；[main.c](lab1/course/src/kernel/main.c) 安排初始化。双核打印和锁的检查已有记录。

### Lab 2：给内核管理内存的能力

在启动基础上增加 `mem/`：[pmem.c](lab2/course/src/kernel/mem/pmem.c) 管理物理内存页，[kvm.c](lab2/course/src/kernel/mem/kvm.c) 管理内核页表。可以把页表理解为“程序使用的地址如何对应实际内存、是否允许访问”的规则表。已检查分配、归还及部分映射；跨核动态页表修改同步等仍有局限，详见复测记录。

### Lab 3：让内核响应发生的事情

扩展 `trap/`：例如 [timer.c](lab3/course/src/kernel/trap/timer.c) 处理时钟，[plic.c](lab3/course/src/kernel/trap/plic.c) 处理外部中断控制器；串口代码在 [lib/uart.c](lab3/course/src/kernel/lib/uart.c)。已检查双核时钟和一组串口输入，尚未覆盖所有异常和长时间运行。

### Lab 4：运行第一个用户程序

新增 [proc/proc.c](lab4/course/src/kernel/proc/proc.c) 管理首个进程、[trap/trampoline.S](lab4/course/src/kernel/trap/trampoline.S) 协助用户态与内核态切换，[trap_user.c](lab4/course/src/kernel/trap/trap_user.c) 处理用户请求和异常；用户程序在 [src/user/initcode.c](lab4/course/src/user/initcode.c)。目前是首个用户进程，并非多进程调度。已测试进入用户态、访问保护和部分分配失败场景。

### Lab 5：让用户程序申请和使用内存

扩展 [mem/uvm.c](lab5/course/src/kernel/mem/uvm.c) 和 [mem/mmap.c](lab5/course/src/kernel/mem/mmap.c)，并增加 [syscall/](lab5/course/src/kernel/syscall/syscall.c) 分派用户请求。`brk` 管理堆边界，`mmap`/`munmap` 管理映射区，用户栈可按规则增长。已有用户复制、堆、映射、栈增长及代码页保护的测试记录。

### Lab 6：让多个进程轮流运行

[proc/proc.c](lab6/course/src/kernel/proc/proc.c) 负责进程创建、等待、退出和调度；[proc/swtch.S](lab6/course/src/kernel/proc/swtch.S) 切换执行现场；[lock/sleeplock.c](lab6/course/src/kernel/lock/sleeplock.c) 提供睡眠锁。可以把调度器理解为“决定哪个进程接下来运行”的内核部分。已记录单核、双核运行以及 `fork`、`wait`、睡眠和抢占等 17 个 QEMU 场景；这些测试不能覆盖所有并发情况。

### 代码怎么一层层连接

Lab 6 从启动到用户程序运行的主要路线是：

```text
QEMU → OpenSBI → boot/entry.S → boot/start.c → main.c 初始化
     → proc/ 创建进程并进入调度 → src/user/initcode.c 运行
     → 遇到 ecall 或中断时进入 trap/ → syscall/ 或相应处理函数
     → 必要时调用 mem/、proc/ 或其他模块 → 返回用户程序
```

各模块的 `type.h` 主要放类型和常量，`method.h` 声明接口，`.c` 和 `.S` 放实现。不是每个阶段都具有上述全部模块：Lab 4 的最小用户调用直接在用户陷入处理路径中处理，独立 `syscall/` 目录从 Lab 5 开始出现。

目前离完整课程交付还差：VisionFive 2 的平台适配和实板启动、课程要求的实验继承分支、部分个人规格说明与更全面的测试。Lab 5、6 的 `%f` 输出已测试，但课程单独列出的 `printfloat(double, int)` 接口尚未提供。具体测试证据和限制以上表链接为准。

## 运行老师的框架

先准备好 RISC-V 交叉编译工具和 QEMU，见 [Lab 0](lab0/README.md)。在项目根目录，例如运行 Lab 6：

```bash
cd /home/zpg/Work/PugeZhang
make -C lab6/course run
```

把 `lab6` 换成 `lab1`～`lab5` 可以运行对应实验。Lab 6 支持 `make -C lab6/course run CPUNUM=1` 或 `CPUNUM=2`。退出 QEMU：先按 `Ctrl+A`，松开后按 `X`。输出与测试入口见上表；多数测试运行后会等待，测试脚本在收集结果后主动停止 QEMU。

Lab 4 的用户页按 RXU 映射；Lab 5、6 目前将用户程序代码放在一个只读可执行页，链接脚本会拒绝需要可写全局数据的程序，演示程序使用栈和堆。Lab 5、6 链接器可能对用户 ELF 提示 RWX 段警告；实际用户页权限和链接段警告是两个不同层面，不能只凭警告判断运行时权限。完整边界与局限见各阶段测试记录。

## 后续工作和硬件

通用内存复制的回归检查可运行 `python3 scripts/check-course-memmove.py`，它直接编译 Lab 2～6 的真实 `utils.c`。重叠复制修复及宿主、RISC-V QEMU 验证见 [2026-11-10 维护记录](docs/evidence/memmove-2026-11-10/README.md)。

- 继续对照老师规格与 PPT 逐项验收，补充压力测试和规格解释；QEMU 的有限测试不能证明所有情况正确。
- 按课程要求整理实验继承分支；当前目录分阶段不等于已经满足分支要求。
- VisionFive 2 需要单独适配启动、内存和设备，并记录真实启动结果。此前写入 microSD 的 Ubuntu 镜像用于开发板环境，**不是我们的内核**。
- 学习时建议依次看 Lab 4 的用户态切换、Lab 5 的用户内存，再看 Lab 6 的 `fork`、调度与 `wait`；详细文件和运行记录在各阶段链接里。

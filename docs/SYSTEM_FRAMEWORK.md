# PugeZhangOS 完整框架与当前进度

本文以仓库现有源码为准，说明项目目录、Lab 1～6 的课程框架、每个内核模块的职责、各部分怎样协作，以及实际测试和未完成的工作。课程原件和各阶段来源见[课程框架记录](COURSE_FRAMEWORK.md)。

**当前状态：** Lab 1～6 的 `course/` 中已填写课程框架，并有 QEMU 构建和场景测试记录。测试通过仅表示记录过的场景通过，不能等同于课程全部验收通过。VisionFive 2 的平台适配和实板启动未完成。代码、测试和部分文档有 Agent 参与。

## 一、整个项目的目录

```text
PugeZhang/
├── README.md                 项目入口
├── ARCHITECTURE.md           旧自建版本的历史架构说明
├── Makefile                  旧自建版本的根目录构建入口
├── docs/
│   ├── SYSTEM_FRAMEWORK.md   本文：当前课程框架总说明
│   ├── COURSE_FRAMEWORK.md   老师各阶段框架的来源、提交号和差异
│   └── COURSE_MATERIALS.md   课件与课程资料索引
├── lab0/                     工具、xv6、开发板资料与操作记录
├── lab1/～lab5/              每个目录同时保留老师框架和早期自建代码
│   ├── course/               老师相应 Lab 的框架及目前填写的代码
│   ├── docs/reference/       课程资料、PPT 或来源记录
│   ├── docs/evidence/        构建、QEMU、调试等实际验证记录
│   └── 其他源码目录          早期自建版本，不是 course/ 的组成部分
├── lab6/
│   ├── course/               老师 Lab 6 框架及目前填写的代码
│   └── docs/                 课程资料与验证记录
├── lab7/～lab9/              项目内后续实验规划目录
├── scripts/                  辅助构建和检查脚本
└── extras/host-shell/        早期宿主 Linux 上的 Shell 练习
```

**课程实验请进入对应的 `labN/course/`。** Lab 1～6 分别保存该阶段的完整源码快照；`lab6/course/` 运行时不会自动加载 `lab5/course/`。早期自建版本中的 `pgos>`、目录和测试，与老师框架分开记录。Lab 7～9 的实际任务和划分应以老师发布的要求为准；当前课程框架还没有实现磁盘、文件系统和用户 Shell。

## 二、系统的层次和整体关系

```text
┌──────────────────────────────────────────────────────┐
│ 用户空间：src/user/                                  │
│ 用户程序 + 系统调用封装                              │
└───────────────────────┬──────────────────────────────┘
                        │ ecall 请求服务；中断和异常也会进入内核
┌───────────────────────▼──────────────────────────────┐
│ 内核空间：src/kernel/                                │
│                                                      │
│ boot/ → main.c                                       │
│            ├─ arch/：CPU 架构、寄存器、SBI           │
│            ├─ lib/：CPU 状态、打印、串口和工具       │
│            ├─ lock/：自旋锁、睡眠锁                  │
│            ├─ mem/：物理页、页表、用户内存和映射     │
│            ├─ trap/：中断、异常、用户态进入与返回   │
│            ├─ syscall/：检查并分发系统调用          │
│            └─ proc/：进程、上下文切换与调度          │
└───────────────────────┬──────────────────────────────┘
                        │ 操作设备或调用固件提供的服务
┌───────────────────────▼──────────────────────────────┐
│ QEMU 中的 RISC-V CPU、内存、串口、中断控制器         │
│ 以及 Lab 2～6 启动时使用的 OpenSBI 固件             │
└──────────────────────────────────────────────────────┘
```

图中是各模块的职责关系，不是开机顺序。Lab 1 使用自己的 M 模式入口，再进入 S 模式；Lab 2～6 在 QEMU 中由 OpenSBI 交接到 S 模式内核。Lab 4 才开始运行 U 模式用户程序，Lab 5 增加独立的系统调用分发目录，Lab 6 才加入多进程调度。

- **QEMU** 是电脑上的模拟器，提供虚拟 CPU、内存和设备。
- **OpenSBI** 是 RISC-V 固件，为内核提供时钟、启动其他核心等底层服务。
- **内核** 管理资源并处理请求；**用户程序**通过受限接口请求内核服务。
- **hart** 是 RISC-V 中的硬件执行线程；本项目的 QEMU 检查通常称它为核心。
- **页表**记录虚拟地址与物理地址之间的映射以及访问权限。
- **trap** 是 CPU 因系统调用、中断或异常转入相应处理入口的机制。

## 三、课程目录内部的完整组织

Lab 1～6 的具体文件随阶段增加。下图列出到 Lab 6 为止的课程框架组成；下节明确每个文件从哪一阶段开始出现。

```text
labN/course/
├── README.md              老师本阶段的说明
├── LICENSE                课程框架许可
├── Makefile               本阶段构建、QEMU 运行和调试规则
├── common.mk              共享编译选项和工具链配置
├── kernel.ld              内核链接布局
├── user.ld                用户程序链接布局（Lab 4 起）
├── registers.xml          GDB 寄存器描述
├── picture/ 或 pictures/  课程说明中使用的图片，按阶段提供
├── spec/                  规格（Lab 4 起）
│   ├── design.yaml        系统总体设计
│   ├── modules/*.yaml     各模块职责与接口规格
│   └── patches/lab*.yaml  各阶段的增量要求与验收场景
├── src/
│   ├── kernel/
│   │   ├── main.c
│   │   ├── arch/          type.h、method.h、mod.h、sbi.c
│   │   ├── boot/          entry.S、start.c
│   │   ├── lib/           cpu.c、print.c、uart.c、utils.c、各头文件
│   │   ├── lock/          spinlock.c、sleeplock.c、各头文件
│   │   ├── mem/           pmem.c、kvm.c、uvm.c、mmap.c、各头文件
│   │   ├── proc/          proc.c、swtch.S、各头文件
│   │   ├── syscall/       syscall.c、sysfunc.c、各头文件
│   │   └── trap/          trap.S、trampoline.S、trap_kernel.c、
│   │                       trap_user.c、timer.c、plic.c、各头文件
│   └── user/              initcode.c、sys.h、syscall_arch.h、syscall_num.h
├── tests/                 部分阶段增加的宿主和 QEMU 辅助测试
└── target/                编译生成的文件；不是源码
```

头文件一般按职责组织：`type.h` 定义类型和常量，`method.h` 声明接口或提供少量内联操作，`mod.h` 汇集模块依赖。`.c` 文件实现 C 逻辑；`.S` 文件实现需要直接操作寄存器和执行现场的汇编逻辑。

## 四、各 Lab 具体有哪些模块

| 模块或文件 | Lab 1 | Lab 2 | Lab 3 | Lab 4 | Lab 5 | Lab 6 |
| --- | --- | --- | --- | --- | --- | --- |
| `boot/entry.S`、`boot/start.c`、`main.c` | 有 | 有 | 有 | 有 | 有 | 有 |
| `arch/` 架构头文件 | 有 | 有 | 有 | 有 | 有 | 有 |
| `arch/sbi.c` 固件服务 | 无 | 有 | 有 | 有 | 有 | 有 |
| `lib/cpu.c`、`print.c`、`uart.c` | 有 | 有 | 有 | 有 | 有 | 有 |
| `lib/utils.c` | 无 | 有 | 有 | 有 | 有 | 有 |
| `lock/spinlock.c` | 有 | 有 | 有 | 有 | 有 | 有 |
| `lock/sleeplock.c` | 无 | 无 | 无 | 无 | 无 | 有 |
| `mem/pmem.c`、`kvm.c` | 无 | 有 | 有 | 有 | 有 | 有 |
| `mem/uvm.c`、`mmap.c` | 无 | 无 | 无 | 无 | 有 | 有 |
| `trap/` 的内核中断处理代码 | 无 | 仅头文件 | 有 | 有 | 有 | 有 |
| `trap/trampoline.S`、`trap_user.c` | 无 | 无 | 无 | 有 | 有 | 有 |
| `proc/` 和 `src/user/` | 无 | 无 | 无 | 有 | 有 | 有 |
| 独立 `syscall/` | 无 | 无 | 无 | 无 | 有 | 有 |
| `spec/` 和 `user.ld` | 无 | 无 | 无 | 有 | 有 | 有 |

“有”表示该阶段的课程快照包含该文件或模块，不表示所有相关要求都已通过验收。Lab 4 的少量系统调用直接在 `trap_user.c` 中处理；Lab 5 和 Lab 6 的系统调用编号也不相同，不能互相混用。

## 五、每个内核文件负责什么

| 文件 | 责任 |
| --- | --- |
| `boot/entry.S` | 内核入口；为各 hart 准备启动栈，并按当前阶段处理启动核与其他核心的入口。 |
| `boot/start.c` | 完成进入内核 C 代码前的架构设置；Lab 1 负责从 M 模式进入 S 模式，后续阶段接收 OpenSBI 交接信息。 |
| `main.c` | 安排共享资源和每核资源的初始化，并在相应阶段启动用户进程或调度器。 |
| `arch/type.h`、`method.h`、`mod.h` | 架构相关类型、寄存器访问和接口。 |
| `arch/sbi.c` | 向 OpenSBI 请求底层服务，包括启动其他 hart 和设置时钟。 |
| `lib/cpu.c` | 获取当前 hart、CPU 状态及当前进程等信息。 |
| `lib/print.c` | 内核格式化输出、打印初始化和 `panic`。 |
| `lib/uart.c` | 串口初始化、字符收发和串口中断处理。 |
| `lib/utils.c` | 内存与字符串等基础工具。 |
| `lock/spinlock.c` | 自旋锁；保护并发访问的共享状态。 |
| `lock/sleeplock.c` | 睡眠锁；竞争者不能立即获得锁时可以等待。 |
| `mem/pmem.c` | 按页管理物理内存，包括分配和释放。 |
| `mem/kvm.c` | 创建内核页表，以及页表项的查找、映射和解除映射。 |
| `mem/uvm.c` | 用户空间数据复制、堆和栈增长、映射与解除、用户页表复制和销毁。 |
| `mem/mmap.c` | 管理记录用户内存映射区的元数据节点。 |
| `proc/type.h` | 进程相关结构、状态以及保存执行现场需要的数据。 |
| `proc/proc.c` | 创建和释放进程；Lab 6 还负责 `fork`、`wait`、`exit`、睡眠、唤醒和调度。 |
| `proc/swtch.S` | 在内核上下文之间保存和恢复执行现场。 |
| `trap/trap.S` | 内核态 trap 入口，保存并恢复相应寄存器。 |
| `trap/trap_kernel.c` | 分派内核态时钟、外部中断及异常，并在 Lab 6 接入可抢占调度。 |
| `trap/trampoline.S` | 用户态与内核态之间切换时，处理用户现场、页表和返回入口。 |
| `trap/trap_user.c` | 处理用户发起的系统调用、用户态中断和异常，并安排返回用户态。 |
| `trap/timer.c` | 时钟事件；Lab 6 接入 tick 和睡眠唤醒。 |
| `trap/plic.c` | 配置和处理 PLIC 外部中断控制器。 |
| `syscall/syscall.c` | 根据用户传来的调用编号，分派到对应内核处理函数。 |
| `syscall/sysfunc.c` | 检查系统调用参数，调用内存、进程或输出模块完成请求。 |
| `user/initcode.c` | 本阶段编译进内核的首个用户程序。 |
| `user/sys.h` | 用户程序调用内核服务时使用的封装。 |
| `user/syscall_arch.h` | 按 RISC-V 调用约定放置参数并执行 `ecall`。 |
| `user/syscall_num.h` | 本阶段系统调用编号。 |

## 六、从源码到内核运行

### 编译和链接

`Makefile` 和 `common.mk` 组织 RISC-V 工具链编译。`kernel.ld` 决定内核入口、代码和数据的链接布局；Lab 4 起的 `user.ld` 决定用户程序布局。Lab 5、6 的构建会编译用户程序，生成二进制并转换成头文件，再把用户映像嵌入内核，最后链接内核 ELF。`target/` 保存这些生成文件。

### 开机和初始化

```text
Lab 1：QEMU → entry.S（M 模式、启动栈）→ start.c → S 模式 main.c
       → 初始化输出和锁 → 各核心运行内核代码

Lab 2～3：QEMU → OpenSBI → entry.S → start.c → main.c
          → 启动核初始化共享资源并启动其他核心
          → 各核心启用自己的页表及相应中断设置

Lab 4～5：启动和初始化 → 建立用户页表、代码页、用户栈和内核栈
          → 创建首个用户进程 → 恢复用户现场 → sret 进入 U 模式

Lab 6：启动核初始化打印、物理页、内核页表、进程和映射节点、trap
      → 创建首个进程并启动其他 hart
      → 各 hart 完成自己的页表、中断和时钟设置 → 进入调度器
      → 选择可运行进程 → 切换现场 → 用户程序运行
```

Lab 4 和 Lab 5 仍是首个用户进程的流程，不具备 Lab 6 的多进程调度能力。Lab 6 的默认用户程序完成输出后继续循环；要观察不同功能，使用测试记录中对应的测试程序。

### 用户请求、异常和返回

```text
用户程序 src/user/ → sys.h / syscall_arch.h → ecall
→ trampoline.S 保存用户现场并进入内核
→ trap_user.c 判断进入原因
→ syscall.c 根据调用编号分发（Lab 5、6）
→ sysfunc.c 检查参数 → mem/、proc/ 或输出模块处理
→ trap_user_return → trampoline.S 恢复现场 → sret 返回用户程序
```

如果是中断或异常，`trap_user.c` 会进入相应处理路径；非法用户访问会停止该用户执行。Lab 5、6 对符合规则的用户栈缺页会尝试增长用户栈。Lab 4 的编号 `0` 直接在 `trap_user.c` 中处理，其余未知编号返回错误。

### Lab 6 的进程和调度

进程是内核正在管理的一份运行中的程序及其资源。进程记录保存编号、状态、页表、内核栈、用户现场和调度需要的上下文。

```text
空闲 UNUSED → 创建中 EMBRYO → 可运行 RUNNABLE ↔ 运行中 RUNNING
                                    ↑                 │
                                    └── SLEEPING ←────┤ 等待后唤醒
                                                      └→ ZOMBIE → wait 回收
```

`fork` 创建子进程并复制需要独立保存的用户地址空间；`wait` 等待子进程并回收；`exit` 结束当前进程；`sleep` 让进程等待；时钟中断和主动让出 CPU 都可能引发调度。两个 hart 可以同时调度不同进程，但同一进程不能同时由两个 hart 执行。

### 系统调用接口

Lab 5 的编号由 [syscall_num.h](../lab5/course/src/user/syscall_num.h) 定义：`0` 问候，`1`～`3` 为用户数据复制相关测试接口，`4` 为 `brk`，`5` 为 `mmap`，`6` 为 `munmap`，`7` 为受检查的字符串输出。

Lab 6 的编号由 [syscall_num.h](../lab6/course/src/user/syscall_num.h) 定义：`1`～`3` 为 `brk`、`mmap`、`munmap`；`4`～`6` 为字符串输出、整数输出、获取 PID；`7`～`10` 为 `fork`、`wait`、`exit`、`sleep`。用户通过寄存器传编号和参数，内核完成后返回结果。

## 七、进度、证据和未完成部分

| 阶段 | 已有实现与记录 | 仍需注意 |
| --- | --- | --- |
| Lab 0 | 工具、xv6、开发板资料和操作记录 | Ubuntu 卡已烧录；我们的内核未在板上启动。 |
| Lab 1 | 启动、输出、自旋锁；有双核 QEMU 检查 | 链接器有 RWX 段警告；无实板验证。[记录](../lab1/docs/evidence/recheck-2026-11-04/README.md) |
| Lab 2 | 物理页和内核页表；有分配、映射和当前核 MMU 检查 | 跨核动态页表修改同步、空页表页回收等存在未实现或未覆盖项。[记录](../lab2/docs/evidence/recheck-2026-11-04/README.md) |
| Lab 3 | 内核时钟、串口和外部中断；有双核及有限输入检查 | 未覆盖全部异常、所有寄存器和长时间并发。[记录](../lab3/docs/evidence/recheck-2026-11-04/README.md) |
| Lab 4 | 首个用户进程、用户态切换、异常处理；测试访问保护及部分分配失败 | 无多进程调度；规格中部分内容由 Agent 起草，需本人理解核对。[记录](../lab4/docs/evidence/course-2026-11-05/README.md) |
| Lab 5 | 用户复制、堆、映射、栈增长和系统调用；有 QEMU 与宿主测试 | 用户代码限单个 RXU 页，可写静态数据被拒绝；未做全部验收及实板测试。[记录](../lab5/docs/evidence/course-2026-11-05/README.md) |
| Lab 6 | 多进程、调度、锁、等待和睡眠；17 个已记录 QEMU 场景通过 | 并发时序和压力测试不穷尽，实板和部分规格仍未完成。[记录](../lab6/docs/evidence/course-2026-11-05/README.md) |
| Lab 7～9 | 当前项目内保留规划 | 课程要求以老师发布内容为准；当前课程框架尚无对应实现。 |

其他待办：

- VisionFive 2 的平台常量仍有 `TODO`，需要按板卡手册、设备树和实际启动环境核对；不能把 QEMU 结果当作实板结果。
- 课程规格中单独列出的 `printfloat(double, int)` 接口尚未提供；Lab 5、6 的 `printf` 中 `%f` 六位输出已有测试。
- Lab 6 的进程现场结构和用户态切换代码当前只保存通用寄存器，未看到浮点寄存器及浮点控制状态的进程级保存与恢复；尚未验证使用浮点运算的多个用户进程切换。已有的浮点打印测试只检查独立探针中调用 `printf` 前后寄存器是否保持，不能证明进程切换会保存浮点现场。
- Lab 6 的页表映射和解除映射会在当前 hart 执行地址转换缓存刷新；代码注释将其他 hart 的失效交给调用者，源码虽提供远程刷新 SBI 封装，当前没有看到调用点。用户态返回时也会在当前 hart 刷新，但尚未验证不同 hart 同时使用并修改共享页表的同步与回收，不能把双核调度测试当作这一能力的验证。
- Lab 6 课程规格对时钟计数核存在“hart 0”和“启动核”两种表述，当前实现使用启动核，需向老师核实。
- 课程要求的实验继承分支尚未整理；按目录划分并不等同于按要求建立分支。
- 部分规格中的学生占位内容、个人理解和实验记录仍需本人完成。不能把 Agent 生成内容描述为本人独立手写。

## 八、运行与退出

在项目根目录启动相应课程框架：

```bash
cd /home/zpg/Work/PugeZhang
make -C lab1/course run
make -C lab2/course run
make -C lab3/course run
make -C lab4/course run
make -C lab5/course run
make -C lab6/course run
```

每行对应一个独立实验，运行时选择其中一行。Lab 6 可附加 `CPUNUM=1` 或 `CPUNUM=2`。退出 QEMU：按住 `Ctrl` 再按 `A`，全部松开，然后按 `X`。实际运行结果与检查脚本见上表中的各阶段记录。

Lab 5、6 的用户程序链接布局可运行 `python3 scripts/check-course-user-layout.py` 检查。脚本会自动寻找支持的 RISC-V assembler 和 linker，也可以通过 `TOOLPREFIX` 指定前缀；检查记录见[工具链维护记录](evidence/toolchain-2026-11-09/README.md)。

Lab 2～6 的内存复制函数可运行 `python3 scripts/check-course-memmove.py` 检查；该检查直接编译课程真实 `utils.c`，覆盖重叠、自拷贝、零长度和范围外保护。RISC-V QEMU 检查入口及修复前后结果见 [memmove 维护记录](evidence/memmove-2026-11-10/README.md)。

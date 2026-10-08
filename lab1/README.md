# Lab 1：让自己的内核启动并打印

这一目录就是完整的 Lab 1。它自己拥有源码、头文件和链接脚本，不需要去 Lab 2 或 Lab 3 找实现，也没有用 `LAB` 宏在一份源码里切换实验。源码按操作系统的模块分工摆放：启动、内核状态、异常、设备驱动、诊断台和测试分别有自己的位置。先把这里读懂，再看 Lab 2 新增的内存管理。

## 这个实验完成什么

QEMU 模拟一台 RISC-V 电脑。电脑刚启动时处于 M 模式，我们先为每个核心准备栈，再设置特权寄存器，通过 `mret` 进入 S 模式的 `kernel_main()`。之后，内核把文字写入 UART 串口，你就在终端里看到输出了。

本实验包含裸机启动、BSS 清零、每核独立栈、M → S 模式切换、串口轮询输入输出、简易 `printf`，以及保护整行打印的自旋锁。它还提供最小的异常报错入口：如果发生意外异常，打印原因和位置后停住。这里不开启时钟和设备中断；完整中断处理在 Lab 3 中。

`pgos>` 是内核自己的命令提示符，它和程序都在 S 模式执行。目前还没有用户进程、用户 Shell、文件系统或任务调度。

## 怎么运行

在仓库根目录执行：

```bash
make -C lab1 qemu
```

默认启动 3 个核心。也可以执行 `make -C lab1 qemu CPUS=1` 或 `CPUS=8`。需要已有 `riscv64-linux-gnu-gcc`、GNU 链接器和 `qemu-system-riscv64`。

终端中会先出现启动信息，每个核心各输出 4 行 `[uart-test]`，接着显示每核的 `[boot]` 记录、`[test] format PASS` 和 `[ready] lab=1 ...`，最后出现 `pgos>`。

输入这些命令即可观察本阶段功能：

```text
help
boot
echo hello-lab1
test
quit
```

`boot` 显示实际走过的阶段、各核栈范围和 `satp`。Lab 1 中 `satp=0x0` 表示分页关闭。`test` 检查数字打印、最小负数、空字符串指针以及输出缓冲区边界。`quit` 写 QEMU 的退出设备并正常结束模拟器。

## 每个模块负责什么

```text
lab1/
├── boot/       最初进入内核，准备栈，切换到 S 模式
├── kernel/     排列启动顺序，保存多核状态，提供自旋锁
├── trap/       发生异常时报告原因并停机
├── drivers/    访问 UART 和 QEMU 退出设备
├── lib/        打印格式处理和字符串辅助函数
├── monitor/    读取命令，编辑输入，展示诊断信息
├── tests/      多核打印检查、格式边界自检
├── include/    模块之间的声明、类型和硬件常量
├── linker.ld   决定内核各段放到哪个内存地址
└── Makefile    只编译本目录的模块
```

`kernel/main.c` 只列出“先做什么、再做什么”。它不负责解析命令，不保存内部启动数组，也不实现测试细节。具体状态由 `kernel/bootstate.c` 拥有；诊断台通过 `boot_snapshot()` 读取一份快照，不直接读写内部数组。这样后续实验增加机制时，可以保留清楚的启动路线。

## 按什么顺序读源码

| 顺序 | 文件 | 先理解的事情 |
|---|---|---|
| 1 | [Makefile](Makefile) | C 和汇编如何编译，链接后如何交给 QEMU |
| 2 | [linker.ld](linker.ld) | 内核从 `0x80000000` 开始，各段按什么顺序放进内存 |
| 3 | [boot/entry.S](boot/entry.S) | 第一个核心清零 BSS，每个核心拿到自己的栈 |
| 4 | [boot/start.c](boot/start.c) | 设置权限、目标地址和目标模式，然后执行 `mret` |
| 5 | [kernel/main.c](kernel/main.c) | 内核依次初始化、等待每核就绪、自检，再进入诊断台 |
| 6 | [kernel/bootstate.c](kernel/bootstate.c) | 主核怎样公布初始化完成，各核心怎样记录阶段并汇合 |
| 7 | [drivers/uart.c](drivers/uart.c) | 内核通过固定地址读写串口寄存器 |
| 8 | [lib/console.c](lib/console.c) | 数字如何拆成字符，格式字符串如何逐项处理 |
| 9 | [kernel/spinlock.c](kernel/spinlock.c) | 多核如何竞争同一把锁，让完整打印不会混在一起 |
| 10 | [trap/trap.c](trap/trap.c)、[trap/entry.S](trap/entry.S) | 意外异常时怎样得到出错原因和指令地址 |
| 11 | [tests/selftest.c](tests/selftest.c) | 如何在启动时检查多核打印和格式化边界 |
| 12 | [monitor/monitor.c](monitor/monitor.c) | 输入怎样累积成一行，退格、超长行和命令怎样处理 |
| 13 | [monitor/diagnostics.c](monitor/diagnostics.c) | 如何读取启动快照并展示每核状态 |

`include/` 放声明、类型和硬件常量。`.h` 告诉其他文件“这个函数存在”；对应的 `.c` 或 `.S` 才是它的实际实现。`lib/memory.c` 目前只有比较字符串和计算长度的辅助函数，不是物理内存管理器。

启动路线可以直接对照源码：

```text
QEMU → _boot → boot_start → mret → kernel_main
                                      ↓
                       UART 初始化 → 公布全局状态
                                      ↓
                     每核安装异常入口 → 每核打印 → 公布就绪
                                      ↓
                   主核等待全部就绪 → 展示快照 → 自测 → monitor_run
```

栈就像一个核心自己的临时工作区。函数调用要在其中保存返回信息和局部变量，因此不同核心不能共用同一块栈。这里每个核心分配 4096 字节，从高地址向低地址使用。

## 怎样验收

在仓库根目录执行以下命令，仅验收 Lab 1：

```bash
python3 scripts/check.py --lab 1 --cpus 1,2,3,8
```

自动检查会启动真实 QEMU，验证 ELF 入口、每核就绪、栈不重叠、分页关闭、每核 4 行打印完整、格式化边界、退格与超长输入处理，以及 `quit` 是否使 QEMU 正常退出。新结果写入本实验的 `docs/evidence/`，见[最新验收汇总](docs/evidence/summary.txt)。也可以执行 `make -C lab1 check` 完成相同检查。

本阶段的小巧思是启动记录：核心确实走过一个步骤，才会设置对应位。主核汇总记录后，你能看到每个核心到底执行到了哪里，而不是只看一行“启动成功”。表中的 `BOOT_TRAP` 仅表示已安装异常诊断入口，不能理解为开启了设备中断。

此次目录拆分、源码精简和自动验证由 Agent 协助完成。个人理解和手动观察记录仍由你自己填写。

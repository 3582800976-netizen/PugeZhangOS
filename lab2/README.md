# Lab 2：给内核建立内存管理

Lab 1 解决“怎样启动并打印”。Lab 2 解决“怎样把空闲内存分给调用者，以及怎样让 CPU 按页表访问内存”。本目录包含这一阶段的完整源码，可以单独编译、运行、阅读；不会链接另一个 Lab 的源码。

## 目录怎样对应操作系统架构

| 目录 | 本阶段负责什么 | 关键入口 |
|---|---|---|
| `boot/` | 固件交接、BSS 清零、每核栈、DTB 数据检查 | `entry.S`、`start.c`、`dtb.c` |
| `kernel/` | 安排启动顺序，记录各核状态，提供自旋锁 | `main.c`、`bootstate.c`、`spinlock.c` |
| `mm/` | 管理物理页、建立和查询虚拟地址映射 | `page.c`、`vm.c` |
| `trap/` | 保存和恢复异常现场，识别受控页异常 | `entry.S`、`trap.c` |
| `drivers/` | 访问 UART 和 QEMU 退出设备 | `uart.c`、`platform.c` |
| `lib/` | 格式化、内存/字符串函数、SBI 固件调用 | `console.c`、`memory.c`、`sbi.c` |
| `monitor/` | 输入和命令分发，展示内核机制的结果 | `monitor.c`、`diagnostics.c` |
| `tests/` | 安排启动自检和多核竞争测试 | `selftest.c` |
| `include/` | 各模块向外提供的接口和硬件常量 | `page.h`、`vm.h`、`bootstate.h` 等 |

`kernel/main.c` 只说明“先初始化谁、再等待谁、最后运行谁”。页表细节在 `mm/`，串口细节在 `drivers/`，命令细节在 `monitor/`。模块私有的自测仍放在实现旁边，例如 `page_selftest()` 在 `mm/page.c`；`tests/selftest.c` 统一调用它们，避免为了测试把内部状态暴露给其他模块。

## 单独运行

在仓库根目录执行：

```bash
make -C lab2 qemu
```

默认 3 核、128 MiB 内存。也可以用 `make -C lab2 qemu CPUS=1`。内核依靠 QEMU 的默认 OpenSBI 固件，从 `0x80200000` 进入 S 模式。`CPUS` 必须与 QEMU 的核数一致，当前支持 1～8 核。

屏幕出现 `pgos>` 后，可输入：

```text
boot
mem
vm
vm 40000000
test
quit
```

`mem` 看两个页池的总页数、空闲页数与非法归还次数。`vm` 看主要地址的映射，`vm 40000000` 看诊断窗口的虚拟地址、物理地址、权限和三级索引。`test` 重跑格式化、物理页和虚拟内存自测。`quit` 让 QEMU 正常退出。输入依靠串口轮询，还没有时钟或设备中断；这里是 S 模式诊断台，还不是用户进程的 Shell。

## 和 Lab 1 的关系

| 机制 | Lab 1 的基础 | Lab 2 的处理 |
|---|---|---|
| 启动模式 | 裸机 M 模式进入，自己执行 `mret` | 改用 OpenSBI，固件完成 M 模式配置，本内核从 S 模式开始 |
| 每核栈与初始化同步 | 每个核心有独立 4 KiB 栈，共享初始化只做一次 | 保留；通过 SBI HSM 启动其他核心 |
| UART、格式化、自旋锁 | 用串口打印，用锁防止多核输出交错 | 保留；串口接收仍然轮询 |
| 物理页分配 | 没有页分配器 | 新增两个页池，每页 4 KiB，每个页池用自己的锁保护 |
| 页表 | `satp=0`，没有分页 | 新增 Sv39 三级页表，每个核心写自己的 `satp` |
| 访问保护 | 没有页权限区分 | 代码 RX、常量 R、数据和空闲 RAM RW |
| 异常 | 仅有诊断基础 | 为真实页权限测试准备最小页异常入口，不开启硬件中断 |

“用户页池”只是将来给用户进程使用的一组物理页。Lab 2 还没有用户进程，也没有为这些页创建 U 模式地址空间。

## 先沿着这条启动路线读

```text
OpenSBI
  → boot/entry.S：选择启动核心、清零 BSS、准备独立栈
  → boot/start.c：确认 S 模式环境，进入 C 内核
  → kernel/main.c：编排 DTB 检查、页池初始化、页表创建
       boot/dtb.c 检查固件数据
       mm/page.c 管理物理页，mm/vm.c 创建页表
  → lib/sbi.c：启动其他核心
  → kernel/bootstate.c：发布全局初始化完成，其他核心等待这一状态
  → 每个核心：打开自己的分页、安装页异常入口
  → tests/selftest.c：多核分配测试、主核自测
  → monitor/：显示结果并运行 pgos> 诊断台
```

DTB 是固件交给内核的硬件说明数据。本实验读取其长度并保留它占用的物理页，避免页分配器把固件数据当成空闲内存覆盖。RAM 的容量和设备地址采用固定的 QEMU virt 配置。

建议按下面的顺序读，不必先读遍所有头文件：

1. [kernel/main.c](kernel/main.c)：先看 `kernel_main()`，理解整个启动和诊断流程。
2. [include/page.h](include/page.h) → [mm/page.c](mm/page.c)：先知道分配/归还接口，再看空闲链表与检查。
3. [include/vm.h](include/vm.h) → [mm/vm.c](mm/vm.c)：先看映射结果，再看 `map_page()`、`vm_init()`、`vm_enable()`。
4. [linker.ld](linker.ld)：看 `_text_start`、`_rodata_start`、`_kernel_end` 怎样划分代码、常量和可分配内存。
5. [boot/entry.S](boot/entry.S) → [boot/start.c](boot/start.c) → [lib/sbi.c](lib/sbi.c) → [kernel/bootstate.c](kernel/bootstate.c)：理解固件和内核怎样分工、其他核心怎样进入并等待共享状态。
6. [trap/trap.c](trap/trap.c) → [trap/entry.S](trap/entry.S)：看页异常验证怎样在故意出错后安全返回。
7. [tests/selftest.c](tests/selftest.c) → [monitor/monitor.c](monitor/monitor.c) → [monitor/diagnostics.c](monitor/diagnostics.c)：最后看怎样验证、操作并观察已经建立的机制。

## 物理页：把空闲内存切成固定大小

物理内存是实际存在的 RAM。分配器从 `_kernel_end` 后开始，将剩余 RAM 大致分成内核页池和用户页池，并跳过 DTB。每一页大小为 4096 字节：

```text
page_alloc(PAGE_KERNEL) → 找到空闲页 → 从链表取走 → 标记已分配 → 清零 → 返回地址
page_free(PAGE_KERNEL, address) → 检查归属和状态 → 填入 0xdd → 放回链表
```

空闲页开头存放“下一个空闲页的地址”，所以链表节点不用额外分配内存。另有位图记录哪些页已经交给调用者，用来拒绝重复归还。未对齐地址、错误页池、内核映像地址、DTB 地址也会被拒绝，失败不会破坏链表。

每个页池有自己的锁。两个池的边界对齐到 8 页，使位图的同一个字节不会被两个不同的锁同时修改。启动时，各个核心会同时申请并保留页，检查地址不重复，再竞争分配与归还，最后检查空闲页数恢复。

## 虚拟内存：CPU 通过三级表找到物理页

虚拟地址是程序指令使用的地址；物理地址是 RAM 或设备实际所在的地址。Sv39 将地址拆成三级索引和页内偏移：

```text
虚拟地址 → VPN[2] 找一级项 → VPN[1] 找二级项 → VPN[0] 找三级项 → 物理页 + 页内偏移
```

页表项除了物理页地址，还带 R（读）、W（写）、X（执行）等权限。本实验全部使用 4 KiB 映射：代码可以读和执行，常量只能读，数据可以读写。UART、PLIC 地址和 QEMU 退出设备进入 RW 映射；PLIC 在本实验只映射，实际中断驱动在 Lab 3 出现。OpenSBI 的内存区域和 CLINT 不进入内核页表。

普通内核映射使用“虚拟地址等于物理地址”，这样打开分页前后，同一指针仍然有效。为说明分页确实做了翻译，额外保留一个诊断窗口：虚拟地址 `0x40000000` 指向另一处物理页。自测通过窗口写入，再从物理页读取；然后反过来做一次。它验证实际 CPU 访问，不只检查页表里的数字。

## 为什么 Lab 2 已经有 trap 文件

为了证明“代码页不能写”，自测会故意执行一次写代码页的指令。CPU 拒绝访问并产生同步页异常，这与 Lab 3 的时钟、串口硬件中断不同：即使关闭 SIE，页异常仍会发生。

`trap/entry.S` 保存寄存器，`trap/trap.c` 检查异常原因、出错指令地址和被访问地址。只有自测明确登记的探针允许恢复，其余异常打印诊断后停机。成功恢复后，寄存器恢复原值，继续自测。这里验证四种真实失败：读零地址、读未映射固件、写代码页、写只读常量。

## 后续实验怎样接在这个框架上

Lab 4～9 会继承已经建立的启动、内存、异常和驱动机制，再按实际课程任务加入新的子系统。`mm/` 管理内存，`trap/` 处理 CPU 进入异常的路径，`drivers/` 处理设备；这些职责不会塞回 `kernel/main.c`。未来的进程、调度和文件等模块，应在对应实验加入实际实现和接口，并由启动程序调用。

本目录固定保留 Lab 2 的可运行版本。后续能力进入后续 Lab，读 Lab 2 时仍只面对这一阶段已经实现的机制。当前没有提前添加空的未来 C 文件或让 Lab 2 引用后续实验源码。

## 验证入口

启动会打印 `[test] page-concurrent PASS`、格式化/页池/页表测试和 `[test] summary failures=0`，然后显示 `[ready] lab=2 ...`。输入 `test` 可重复验证；重复自测不会额外占用页。

在仓库根目录可运行完整自动检查：

```bash
python3 scripts/check.py --lab 2 --cpus 1,2,3,8
```

最新分目录版本的检查结果和原始日志保存在本阶段的 [docs/evidence/summary.txt](docs/evidence/summary.txt)。

实现和自动测试由 Agent 协助完成。个人学习记录保留在上层 `docs/MY_LAB_NOTES.md`；理解这份实验时，从上面的启动路线和 `mem`、`vm` 输出开始即可。

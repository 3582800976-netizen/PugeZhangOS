# Lab 4：第一个用户进程

本阶段以 Lab 3 为基础，新增进程、用户页表、用户陷入入口和最小系统调用。**2026-10-13，QEMU 的 1、2、3、8 核检查全部通过**，见[验收汇总](docs/evidence/summary.txt)。实体开发板配置暂缓，个人契约解释和手动操作仍由本人完成。

课程原件：[Lab 4 PPT](docs/reference/拔尖班-lab-4.pptx)，共 9 页。[来源与校验](docs/reference/README.md)。第 4 页介绍 PCB，第 5 页介绍地址空间，第 6 页介绍首次执行与调用返回，第 7–9 页要求先写规格、再生成代码。

寄存器与陷入行为依据 [RISC-V Supervisor 规范](https://docs.riscv.org/reference/isa/priv/supervisor.html)和 [Machine 规范的 ECALL 说明](https://docs.riscv.org/reference/isa/priv/machine.html)。

## 先理解这次多了什么

Lab 3 的所有代码都在内核的 S 模式中执行。Lab 4 增加一个单独编译的小程序，让它以权限更低的 U 模式执行。它只能直接访问自己的代码、数据和栈。

PCB 可以理解为“这个程序的资源登记表”：记录进程编号、页表、用户栈、内核栈和保存现场的位置。页表像地址的通行规则：有些页能读写，有些只能读或执行，有些没有映射。

用户程序打印时执行 `ecall`，请内核帮忙。CPU 转到 S 模式，入口汇编保存用户寄存器、换成可信内核栈和页表，然后 C 代码检查请求。完成后恢复用户现场，执行 `sret` 回到 U 模式，继续原来的程序。

`trapframe` 保管被打断的用户现场；`context` 保管内核函数切换所需的现场。`context_switch` 用于进入首个进程的启动路径和结束后回到诊断台，`sret` 才完成从 S 到 U 的权限切换。

错误访问采用“结束这个进程，内核继续工作”。例如用户直接读取 UART 时，页异常记录实际地址和指令位置，然后回到 `pgos>`。用户通过受检查的系统调用依然可以打印。

## 运行与命令

在仓库根目录执行：

```bash
make -C lab4 qemu CPUS=3
```

支持 1–8 核；只有启动主核执行这个用户进程，UART 中断也路由给它，便于观察用户期间的外部中断。所有核心继续处理各自的时钟。本阶段没有多进程调度。

启动后的 `pgos>` 仍是 S 模式内核诊断台。输入：

```text
user
proc
user 2
echo kernel-still-alive
utest
mem
quit
```

- `user`：运行正常用户程序，计算 1 加到 100 得到 5050，测试打印、系统调用返回和真实用户时钟中断，再退出。
- `proc`：展示最近一次真实运行的结果。
- `user 2`：运行直接读取 UART 的程序，观察页异常及返回诊断台。
- `utest`：运行正常/错误程序、地址权限、分配失败和资源回收检查。
- `test`：继续运行原有格式化、物理页、内核页表和中断现场检查。

每次运行前的 `[user-start]` 展示真实 PID、核心、用户入口、用户栈、内核栈和页表根。运行后的 `[proc]` 展示 U 来源、退出原因、计算结果、调用次数和用户中断次数。发生保护异常时另有 `[proc-fault]` 的 `scause`、`sepc`、`stval`，分别对应“发生什么”“哪条指令”“哪个地址”。

| 模式 | 行为 | 预期结果 |
| --- | --- | --- |
| 0 | 正常计算、调用与定时器 | 退出 0，报告 5050，实际用户 timer 中断不少于 3 |
| 1 | 读取内核代码地址 | load page fault，cause 13 |
| 2 | 直接读取 UART | load page fault，cause 13 |
| 3 | 修改自己的只读代码 | store page fault，cause 15 |
| 4 | 从不可执行的用户栈取指 | instruction page fault，cause 12 |
| 5 | 读取受保护 trapframe | load page fault，cause 13 |
| 6 | 读取 S-only trampoline | load page fault，cause 13 |
| 7 | 错误地址、参数和未知系统调用 | 返回错误码，用户程序仍能退出 0 |

## 文件与执行路线

| 文件 | 职责 |
| --- | --- |
| [spec/design.yaml](spec/design.yaml) | 平台、机制和全局规则 |
| [spec/modules/](spec/modules/) | 函数的调用前条件、作用和完成后状态 |
| [spec/patches/lab4.yaml](spec/patches/lab4.yaml) | 本实验增量和验收场景 |
| [proc/process.c](proc/process.c) | PCB、首次执行、结束和资源归还 |
| [mm/user_vm.c](mm/user_vm.c) | 独立用户页表、权限和安全复制 |
| [trap/context.S](trap/context.S) | 内核 context 的保存和恢复 |
| [trap/user_entry.S](trap/user_entry.S) | 共享跳板，保存/恢复全部用户整数寄存器 |
| [trap/user_trap.c](trap/user_trap.c) | 用户调用、中断、异常和返回 |
| [syscall/dispatch.c](syscall/dispatch.c) | 最小请求的参数检查和执行 |
| [user/first.c](user/first.c) | 独立编译的用户 C 程序 |
| [user/register_probe.S](user/register_probe.S) | 检查调用往返是否保持寄存器，包括用户 `tp` |
| [tests/user_process.c](tests/user_process.c) | 功能、权限、失败回滚与回收验证 |

```text
内核初始化 → 建立固定进程槽 → pgos> 输入 user
  → 创建 PCB 和用户页表
  → context_switch → 首次返回路径 → trampoline → sret
  → 用户计算 / ecall / 中断 / 错误访问
  → user_vector 保存用户现场，换页表和栈
  → user_trap → 系统调用处理后返回，或结束进程
  → context_switch 回到诊断台 → 释放用户页和页表
```

## 地址与资源如何保护

使用 Sv39 正半区，`VA_MAX = 1 << 38`。以下布局是本项目设计，参考课件说明；未原样复制 xv6 源码。

| 区域 | 地址 | 权限与归属 |
| --- | --- | --- |
| 空指针页 | 0 | 不映射 |
| 用户代码和常量 | `0x1000`，一页 | RXU，用户拥有 |
| 用户数据与 BSS | `0x2000`，一页 | RWU，用户拥有 |
| 堆起点 | `0x3000` | 本阶段不增长，不映射 |
| trampoline | `VA_MAX - 4096` | RX，无 U，借用内核代码 |
| trapframe | `VA_MAX - 8192` | RW，无 U，固定槽拥有 |
| 用户栈 | `VA_MAX - 12288` | RWU，无 X，用户拥有 |
| 栈保护页 | `VA_MAX - 16384` | 不映射 |
| 内核栈 | `VA_MAX - 20480` | RW，无 U，固定槽拥有 |

内核和用户页表都在同一高地址映射 trampoline，使切换页表时仍能执行跳板代码。用户页表没有内核 RAM 和设备的 identity 映射。

固定槽初始化时永久保留一页 trapframe 和一页内核栈，及其内核映射所需页表；每次运行再申请三个用户页和五个用户地址空间页表页。结束后归还临时的八个页，永久资源保持有效。测试基线在初始化之后取样，避免把保留资源误判成泄漏。

用户 `tp` 可以是任意值，不能当作核心编号。入口先保存它，再从 S-only trapframe 取得可信内核核心编号；因此用户无法通过改 `tp` 干扰锁和中断计数。

## 最小系统调用

这些编号是本项目的演示接口，详细后续要求仍需核对课件。`a7` 传编号，`a0` 等寄存器传参数，`a0` 返回结果。

| 编号 | 调用 | 行为 |
| --- | --- | --- |
| 1 | GETPID | 返回当前进程编号 |
| 2 | WRITE | 检查并复制最多 128 字节用户数据，再输出 |
| 3 | REPORT | 记录用户计算结果，供内核核对 |
| 4 | EXIT | 结束当前进程，返回诊断台 |
| 5 | TICKS | 返回本进程实际在 U 模式发生的 timer 中断数 |

WRITE 逐页检查完整范围，拒绝溢出、未映射和没有 U/R 权限的地址；检查失败时不输出前半段。坏地址返回 `-14`，长度不合法返回 `-22`，未知调用返回 `-38`。只对明确的 `ecall` 推进 PC 四字节；中断返回原 PC，保护异常结束程序。

## 验收与本人参与

```bash
make -C lab4 check
python3 scripts/check.py --lab 4 --cpus 1,2,3,8
```

验证记录放本阶段 `docs/evidence/cpuN/`，本实验汇总放 `docs/evidence/summary.txt`。检查还在真实 U 执行期间注入串口命令，验证 PLIC 中断和恢复后输入处理。最终状态以实际运行结果为准。

构建固定使用 `-mcmodel=medany`、`-mno-relax` 和整数 ABI；当前内核没有依赖 `gp` 寻址，也没有浮点现场。若以后改变这些编译约束，须重新审核用户进入内核时的寄存器契约。

规格提交先于源码实现。该规格由 Agent 起草，函数解释和本人操作尚未冒充完成：[分工说明](spec/README.md)。你可以亲手运行上面的正常程序和 UART 错误程序，观察相同系统如何处理两种结果，再用自己的话说明 `user_vm_create`、`process_run`、`trap_user_return`。真实内容写到本阶段 `docs/NOTES.md`，不预填个人经历。

[系统框架](../ARCHITECTURE.md) · [后续 Lab 5](../lab5/README.md)

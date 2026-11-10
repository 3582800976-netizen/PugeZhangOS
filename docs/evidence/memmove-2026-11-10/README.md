# memmove 重叠复制修复与验证：2026-11-10

本次修复 Lab 2～6 `course/src/kernel/lib/utils.c` 的同一错误。旧函数始终从前向后复制，目标位于源后方且区间重叠时，会覆盖尚未读取的字节。Lab 4～6 的 `spec/modules/console.yaml` 明确要求允许任意重叠。

修复保持老师的 `void memmove(void *, const void *, uint32)` 接口：零长度或自拷贝立即返回；目标地址较高时从尾向前复制，其余情况向前复制。比较使用项目的 RV64 整数地址类型，避免对不同对象的 C 指针直接作关系比较。这是正确性修复，不声明性能提升。

## 真实实现的回归检查

[修复前](before.log)在五个阶段均失败；[修复后](after.log)每阶段均通过 187,334 个快照用例，并另行覆盖空指针零长度与不同对象之间的复制。用例枚举小缓冲区的全部合法偏移和长度，包含两个重叠方向、自拷贝、相邻区间及目标区外保护；大缓冲区还检查跨 4 KiB 边界和两种复制方向。

测试编译课程的真实 `utils.c`，仅重命名导出符号以免与宿主 libc 冲突。预期结果从调用前的独立快照构造，没有使用 `memmove` 替身。[测试程序](../../../scripts/tests/memmove_host.c)和[运行脚本](../../../scripts/check-course-memmove.py)保留在仓库中。

在仓库根目录复现：

```bash
python3 scripts/check-course-memmove.py
CC='cc -fsanitize=address,undefined' ASAN_OPTIONS=detect_leaks=0 \
  python3 scripts/check-course-memmove.py
```

[检测器日志](sanitizers.log)记录五阶段通过地址和未定义行为检测；本次没有启用 LeakSanitizer。

## RISC-V 目标代码检查

[目标测试脚本](../../../scripts/check-course-memmove-qemu.py)在临时课程副本中，将[快照检查](../../../scripts/tests/memmove_kernel.h)加入初始化后的内核入口。它使用正常 RISC-V 工具链编译真实 `memmove`，在 Lab 2～6 的两核 QEMU 中各运行 3,570 个快照用例。正式 `main.c` 不变。

```bash
python3 scripts/check-course-memmove-qemu.py --out-dir /tmp/puge-memmove-rv64
```

结果见 [汇总](rv64-summary.log)及 [RISC-V 构建与运行原始日志](rv64/)。取得目标标记后由脚本终止 QEMU，SIGTERM 不表示系统自行关机。此检查不代替开发板实测。

## 现有功能回归

- [Lab 1～3 汇总](lab123-summary.log)及 [原始日志](lab123/)：复用既有检查，涵盖双核构建、内存映射、时钟和串口。Lab 1 由共用脚本顺带复测，源码未改动。
- [Lab 4 汇总](lab4-summary.log)及 [运行日志](lab4/)：用户态、中断和非法用户访问两个既有场景通过。
- [Lab 5 汇总](lab5-summary.log)及 [原始日志](lab5/)：用户堆与映射、用户内存边界、代码页写保护三个既有场景通过。
- [Lab 6 汇总](lab6-summary.log)及 [原始日志](lab6/)：既有 17 个 QEMU 场景通过，包含进程、内存隔离、等待、睡眠及抢占。
- [Lab 5 宿主测试](lab5-host.log)和 [Lab 6 宿主测试](lab6-host.log)通过。

这些结果只覆盖本次列出的场景。指令同步、页表页延迟回收、浮点进程现场及其他已记录的课程待办仍需分别处理。

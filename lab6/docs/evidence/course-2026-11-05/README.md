# Lab 6 课程框架检查：2026-11-05

本次在 `lab6/course/` 的老师框架内实现进程创建和回收、调度、`wait`/`exit`、睡眠唤醒、锁、时钟抢占，并衔接用户地址空间和系统调用。以下是已观察到的场景，不代表所有并发时序或课程全部验收都已覆盖。

在仓库根目录复现：

```bash
make -C lab6/course build CPUNUM=2
python3 lab6/docs/evidence/course-2026-11-05/run-qemu-checks.py \
  --course lab6/course --out-dir lab6/docs/evidence/course-2026-11-05
(cd lab6/course && sh tests/run-host.sh)
python3 scripts/check-course-user-layout.py
python3 lab6/course/tests/printf_float/run.py
```

[QEMU 汇总](summary.txt)中 17 项通过；每项的 `*-build.log`、`*-runtime.log` 是构建和运行原始输出。包括：

- 单核和双核启动、`fork`、`wait`、父子堆/栈/映射隔离，以及等待重试、进程数限制。
- 用户内存操作、非法访问和代码页写入保护；串口用户态中断。
- 创建过程指定分配点失败后的回收与重试；锁竞争、睡眠唤醒和至少 30 个时钟滴答。
- 单核运行中内核态时钟抢占；双核进程所有权检查，覆盖有限次数的切换。

脚本在临时课程副本中编译测试程序，少数场景应用同目录的测试专用 `.patch` 以插入标记或错误注入；达到预期输出后停止 QEMU，SIGTERM 不表示正常关机。

[宿主测试](host-tests.log)覆盖锁、页分配、页表、用户复制及映射区操作，使用了模拟对象，不等同于真实硬件或完整并发证明。[浮点测试](float-tests.log)比较 3000 个有限 binary64 值，并在独立的 RISC-V QEMU 探针中检查 `printf` 调用前后浮点寄存器保持；它没有运行用户进程调度，不能证明多进程切换时浮点现场得到保存。[GDB 记录](gdb.log)在 `_entry` 的 `0x80200000` 命中断点。

[用户布局检查](user-layout.log)记录可写全局变量被链接脚本拒绝：当前用户程序只映射一页 RXU 代码，可写内容应放在栈或堆。用户 ELF 的 RWX 段链接警告与运行时页表权限需要分别理解。

**剩余边界：** 没有 VisionFive 2 实板运行结果；尚未穷举并发时序、长时间压力测试和全部课程规格。课程 `spec/design.yaml` 对时钟计数核写 hart 0，`spec/modules/trap.yaml` 和增量规格写启动核；当前采用启动核，待与老师核实。部分规格中的学生占位内容还需填写和本人理解，代码与测试有 Agent 参与。

当前 [进程现场结构](../../../course/src/kernel/proc/type.h)和[用户态切换代码](../../../course/src/kernel/trap/trampoline.S)只记录通用寄存器，未看到进程级浮点寄存器和浮点控制状态的保存与恢复；使用浮点运算的多用户进程切换尚未验证。

[页表映射代码](../../../course/src/kernel/mem/kvm.c)会在当前 hart 刷新地址转换缓存，并注明其他 hart 的失效由调用者处理。源码虽然提供远程刷新 SBI 封装，当前未找到其调用点；用户态切换也会在当前 hart 刷新。双核调度测试没有验证不同 hart 同时使用并修改共享页表时的同步与物理页回收。

课程 `console.yaml` 还列出 `printfloat(double, int)` 的具名接口；当前实现是内部 `printfloat_bits`，已经测试的是 `printf` 的 `%f` 六位输出，尚未验证或提供那个独立具名接口。

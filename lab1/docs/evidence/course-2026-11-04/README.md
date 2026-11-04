# Lab 1 课程框架：2026-11-04 验证记录

工作目录是 `lab1/course/`，基于老师 Lab 1 提交 `e1f2efd5bcccf6a7f991192edc2ee8057844ed93`。Agent 填写 `src/kernel/boot/start.c`、`src/kernel/main.c`、`src/kernel/lock/spinlock.c`、`src/kernel/lib/print.c`，并调整 Makefile 构建目录依赖和双核限制；课程接口和目录保留。个人理解和手动运行不冒充已经完成。

## 构建和启动

- 在正式工作目录执行 `make clean build`，成功；实际输出见 [build.log](build.log)。原课程链接脚本产生 LOAD 段 RWX 警告，尚未修改。
- 运行 `qemu-system-riscv64 -machine virt -bios none -kernel target/kernel/kernel-qemu.elf -m 128M -smp 2 -nographic`，看到两个核心各自输出启动行，见 [boot-runtime.log](boot-runtime.log)。
- `make -n run CPUNUM=3` 被拒绝，见 [invalid-cpu.log](invalid-cpu.log)。老师框架为两个核准备了栈；直接绕过 Makefile 使用更多核仍未验证。

## 在临时副本检查锁与格式

测试在 `/tmp/puge-course-lab1-validation` 临时副本进行，正式 `main.c` 没有换成测试代码。以 [main-locked.c](main-locked.c) 替换临时副本的 `src/kernel/main.c`，执行 `make build` 和上面的 QEMU 命令：两核各打印 20 条完整测试行，各加锁递增 1,000,000 次，最终 `sum=2000000`；锁持有状态、嵌套关中断恢复及打印格式检查均符合测试预期，见 [lock-runtime.log](lock-runtime.log)。课程框架约定的 `%p` 和 `%x` 含义见课程 `print.c` 注释，与标准 C 的习惯不同。

以 [main-unlocked.c](main-unlocked.c) 做未加锁对照，一次运行的结果为 `1034220`，见 [unlocked-runtime.log](unlocked-runtime.log)。并发结果不固定，此对照发生在最后调整 panic 输出之前。测试副本还用 [panic-probe.py](panic-probe.py) 验证递归获取锁和不配对的 `pop_off`，日志分别为 [panic-recursive.log](panic-recursive.log) 和 [panic-bad-pop.log](panic-bad-pop.log)。**探针会覆盖当前目录的 `src/kernel/main.c`，只能在临时副本运行。**

测试脚本在收集目标输出后主动结束 QEMU；日志中的 SIGTERM 不代表内核自主关机。以上是 QEMU 双核结果，未验证开发板；老师模板的 `make debug` 缺少 `.gdbinit.tmpl-riscv`，本次未修复或验证，早期异常诊断也仍有限。

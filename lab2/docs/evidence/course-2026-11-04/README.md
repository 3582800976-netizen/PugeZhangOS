# Lab 2 课程框架：2026-11-04 验证记录

工作目录是 `lab2/course/`，基于老师 Lab 2 提交 `79b9ad32fca56697e8d904f349a151444e3fdcd8`。Agent 衔接课程 Lab 1 的打印与锁，并填写启动、物理页和页表相关代码；实际修改为 `src/kernel/boot/entry.S`、`boot/start.c`、`lib/print.c`、`lock/spinlock.c`、`main.c`、`mem/pmem.c` 和 `mem/kvm.c`。个人理解不冒充已经完成。

## 构建与启动

在课程工作目录执行 `make build`，见 [build.log](build.log)。默认 QEMU 使用 OpenSBI、130 MiB 和两个核心：

```bash
make -C lab2/course run
```

正式工作目录实际运行的 [boot-runtime.log](boot-runtime.log) 记录了两个核心的启动输出。OpenSBI 可以选择 0 或 1 作为启动核；本次日志是 0，另一种启动核已有临时运行观察，但这不等于完整验证所有启动情况。

## 临时测试副本

在 `lab2/course/` 的临时副本中应用 [test-only.patch](test-only.patch)，分别执行 `make clean`、`make build`，再运行：

```bash
timeout 8s qemu-system-riscv64 -machine virt -bios default \
  -kernel target/kernel/kernel-qemu.elf -m 130M -smp 2 -nographic
```

补丁只在测试副本添加测试入口、测试程序和锁内读取空闲页数的辅助函数。构建和实际输出见 [test-build.log](test-build.log)、[test-runtime.log](test-runtime.log)。运行检查包括：两核 Sv39、100 轮并发页分配、申请 10 页前后空闲数变化和归还恢复、页面清零复用、PTE 映射与解除、当前核心的硬件地址别名读写、设备树魔数，以及继承的打印和锁检查。退出码 124 和日志中的 SIGTERM 来自 `timeout` 停止空转的模拟器，不是内核自主关机。

另外在两个独立临时副本分别测试页池耗尽：使用 [kernel-test-only.patch](kernel-test-only.patch) 和 [user-test-only.patch](user-test-only.patch)，检查空闲数和清零后持续申请，直到各自输出资源耗尽的 panic。构建及运行结果分别为 [内核页池构建](kernel-build.log)、[内核页池输出](kernel-runtime.log)、[用户页池构建](user-build.log)、[用户页池输出](user-runtime.log)。测试代码改变了镜像大小，日志里的页数不是正式内核固定的总页数；panic 后由 `timeout` 结束 QEMU。

## 尚未完成的检查和限制

- 这组测试不代表课程全部验收完成；设备树完整内容及非法输入、硬件权限故障尚未覆盖。
- 跨核动态更改页表时的 TLB 同步和空页表页回收尚未实现；本次动态映射测试只涉及当前核心。
- 课程原 Makefile 的 `make clean build` 组合调用存在目录创建问题，请分开执行 `make clean` 和 `make build`。
- 上述结果来自 QEMU，尚未在 VisionFive 2 上运行。

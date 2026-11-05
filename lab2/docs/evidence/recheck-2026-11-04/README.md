# Lab 2 课程框架复查

2026-11-04 在临时副本重新编译与运行课程 Lab 2。双核检查覆盖物理页并发分配、10 页计数减少与恢复、归还后清零复用、页表映射和解除、当前核心实际 MMU 别名读写、DTB 魔数以及 Lab 1 锁和打印回归；见[运行记录](runtime.log)和[测试构建](test-build.log)。测试入口和资源检查由此前归档的 `test-only.patch` 临时加入，没有放进正式内核。

修复 Makefile 在已有 `target/` 时 `make clean build` 无法重建目录的问题，以及并行编译时目录创建竞态；补齐头文件依赖、链接脚本依赖并固定默认目标。先有产物再执行 `make clean build`、单独清理后执行 `make -j8 build` 均成功，见[组合构建](clean-build.log)和[并行构建](parallel-build.log)。实验当前仅支持两核，`CPUNUM=3` 已明确拒绝，见[拒绝记录](invalid-cpus.log)。

`make debug` 已不再依赖缺失的模板文件；通过本机 GDB 实际命中 `_entry`，PC 为 `0x80200000`，见 [GDB 输出](gdb.log) 和 [QEMU 输出](gdb-qemu.log)。完整重跑方式见 [Lab 1 复查脚本](../../../../lab1/docs/evidence/recheck-2026-11-04/README.md)。

QEMU 在测试输出后由脚本发送 SIGTERM 结束。跨核动态页表修改的 TLB 同步、空页表页回收、硬件权限故障和设备树非法输入仍未覆盖或未实现；未在实体开发板上验证。

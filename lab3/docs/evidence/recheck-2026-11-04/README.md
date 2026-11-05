# Lab 3 课程框架复查

2026-11-04 在临时副本重新编译和运行课程 Lab 3。两个核心分别报告时钟中断与代表性寄存器恢复通过；通过真实 QEMU 串口连续输入字母、数字、退格、`!` 和回车，观察预期回显，随后静置一秒无额外输出。见[运行记录](runtime.log)和[测试构建](test-build.log)。测试入口和计数由此前归档的 `test-only.patch` 临时加入，没有放进正式内核。

修复 Makefile 在已有 `target/` 时 `make clean build` 的目录问题，以及并行编译目录竞态；补齐链接脚本依赖并固定默认目标。组合构建、独立清理后的 `make -j8 build` 均成功，见[组合构建](clean-build.log)和[并行构建](parallel-build.log)。实验当前仅支持两核，`CPUNUM=3` 已明确拒绝，见[拒绝记录](invalid-cpus.log)。

`make debug` 已不再依赖缺失的模板文件；本机 GDB 实际命中 `_entry`，PC 为 `0x80200000`，见 [GDB 输出](gdb.log) 和 [QEMU 输出](gdb-qemu.log)。完整重跑方式见 [Lab 1 复查脚本](../../../../lab1/docs/evidence/recheck-2026-11-04/README.md)。

QEMU 在测试输出后由脚本发送 SIGTERM 结束。这里只检查了代表性寄存器和有限串口输入；用户态陷入、全部异常及长时间并发未覆盖，Lab 2 的动态页表限制仍然存在，未在实体开发板上验证。

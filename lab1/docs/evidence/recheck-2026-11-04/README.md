# Lab 1 课程框架复查

2026-11-04 使用 RISC-V GCC、QEMU 及 `gdb-multiarch` 复查；代码和检查由 Agent 完成。结果来自 QEMU，不代表开发板实测。

- 重新编译并用临时测试程序检查两个核心的打印、自旋锁和中断嵌套恢复：两核分别加锁递增 1,000,000 次，结果为 2,000,000，40 行输出均完整。见 [运行记录](runtime.log) 和 [测试构建](test-build.log)。
- 在实际入口读取 hart ID 后、访问启动栈之前，增加 `NCPU=2` 上限保护。直接启动三核 QEMU 时，只有 0、1 号核心进入内核并输出，见 [额外核心测试](extra-hart.log)。课程 Makefile 仍只支持两核，这个测试不表示支持三核工作。
- 修复课程模板 `make debug` 对缺失文件的依赖，仅在本机回环端口开启 GDB。通过 GDB 在 `_entry` 命中断点、读到 PC `0x80000000`，见 [GDB 输出](gdb.log) 和 [QEMU 输出](gdb-qemu.log)。

完整重跑脚本见 [run-recheck.py](run-recheck.py)：在仓库根目录执行 `python3 lab1/docs/evidence/recheck-2026-11-04/run-recheck.py`，脚本在临时目录构建并测试 Lab 1～3，日志写入 `/tmp/puge-recheck-123/`。GDB 复查可运行 [gdb-recheck.py](gdb-recheck.py)，需要可用的本机 TCP 回环端口 26141～26143；脚本会启动并结束 QEMU。

内核启动后持续等待；测试程序在获得目标输出后发送 SIGTERM 结束 QEMU。Lab 1 课程链接脚本仍有 RWX 段警告，未在实体板上验证。

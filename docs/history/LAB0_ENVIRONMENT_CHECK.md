> 历史环境检查记录：你随后已经亲手运行 xv6。当前入口见 [Lab 0](../../lab0/README.md)。

# Lab 0：环境正确，还要本人复现

检查日期：2026-10-04。

## 结论与证据

本地有 QEMU 5.1.0、RISC-V GCC 13.3.0 和 `gdb-multiarch`；参考仓库在 `util` 分支。

Agent 在 `/tmp` 中复制参考源码，排除旧目标文件和可执行文件，重新执行 `make -j2 kernel/kernel fs.img`，退出码为 0。随后启动三核 QEMU，得到：

```text
xv6 kernel is booting
hart 2 starting
hart 1 starting
init: starting sh
$ echo LAB0_OK
LAB0_OK
$ ls
...
README ...
sh ...
```

使用 QEMU 的退出快捷键正常结束，退出码为 0。原参考源码和你自己的内核源码未修改。

完整证据：[编译记录](../evidence/lab0-build.log)、[运行记录](../evidence/lab0-runtime.log)。这些是 Agent 的验证，不是本人操作记录。尚未测试 GDB 实际连接或实体开发板。

参考 xv6 的 Makefile 已有告警策略调整：去掉通用 `-Werror`，加入 `-Wno-error=infinite-recursion`。本次证明的是当前本地版本能编译运行，不是原版在零调整下兼容本机编译器。

## 哪些留，哪些不用重来

| 内容 | 处理 |
| --- | --- |
| QEMU、GCC、GDB 和现有开发环境 | 保留；运行验证通过，无须重装 |
| `xv6-labs-2020` | 保留为参考和环境验证对象 |
| `qemu-5.1.0` | 保留为工具源码及构建结果；不加入自己内核的源码树 |
| `PugeZhang` | 保留项目名称与历史，逐步整理成自己的内核仓库 |
| 现有内核二进制和输出 | 保留为历史证据；不代替从源码编译和本人验收 |

## 本人现在要做的第一步

在自己的终端逐条执行，不把提示符 `$` 当作命令内容：

```sh
cd /home/zpg/Work/xv6-labs-2020
git branch --show-current
make qemu
```

第一条 Git 输出应为 `util`。看到 xv6 的 `$` 后，输入：

```text
echo hello-xv6
ls
```

退出：先按 `Ctrl+A`，松开，再按 `X`。

在 [本人记录](../MY_LAB_NOTES.md) 中记录你看到的输出，再用自己的话回答：

1. GCC、QEMU、xv6 各负责什么？
2. 你刚才看到的 `$` 属于宿主 Linux，还是 QEMU 中的 xv6？
3. 为什么 `PugeZhang` 的内核输出不能替代 xv6 的环境验证？

这一轮只需要完成这些，不用立即阅读 xv6 的进程和文件系统代码。

## Lab 0 完整完成度

工具运行验证通过。本人理解和亲手复现待完成。已有 README 和代码仓库，但课程要求的公开仓库可访问性、共享文档登记状态未核实，不能自动宣称这些都已完成。

课程参考：[Lab 0 课件](https://gitee.com/christinaaa/ecnu-oslab-2026-task/blob/master/lab-ppt/拔尖班-lab-0.pptx)。

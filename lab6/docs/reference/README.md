# Lab 6 课程资料归档

本目录归档了课程 Gitee 仓库 `lab6` 分支在提交 `7effc11729eaeb2231d49f0cdcf41619da5e9ca4` 的完整教学资料，共 82 个文件、约 1.8 MB。离线入口是[课程 README](course/lab6/README.md)；同目录保留了课程规格、源码框架、构建文件和验收图片。

## 为什么没有 Lab 6 PPT

核对时，默认分支 `master/lab-ppt` 只列到 Lab 5；`lab6` 分支也没有 PPT 或 PDF。老师把 Lab 6 材料放成 Markdown 指导书、YAML 规格、源码框架和 `picture/` 下的状态图与测试输出图。因此这里归档的是仓库实际发布的全部 Lab 6 教学文件，不会把 README 改名成 PPT。

资料来源：[课程仓库 Lab 6 分支](https://gitee.com/christinaaa/ecnu-oslab-2026-task/tree/lab6)。网页会触发安全验证码，文件通过 Gitee raw 直链取得；API 目录清单核实了文件范围。文件固定在上述提交，避免后续分支变化悄悄改写本地资料。

## 目录

| 路径 | 内容 |
|---|---|
| [`course/lab6/README.md`](course/lab6/README.md) | 实验指导、状态机说明、实现提示和 4 组用户态测试 |
| [`course/lab6/spec/design.yaml`](course/lab6/spec/design.yaml) | Lab 6 总体设计及非目标 |
| [`course/lab6/spec/patches/lab6.yaml`](course/lab6/spec/patches/lab6.yaml) | 本阶段改动和验收要求 |
| `course/lab6/spec/modules/` | 进程、锁、系统调用、陷入、内存等模块契约 |
| `course/lab6/src/` | 课程源码框架；保留在参考目录，不等同于本项目 Lab 6 实现 |
| `course/lab6/picture/` | 进程状态图和测试预期输出图 |

## 完整性与构建检查

`SHA256SUMS.course-lab6` 为 82 个归档文件提供校验值；从本目录运行 `sha256sum -c SHA256SUMS.course-lab6` 可复核。

本次归档、课件校验和基线测试结果见[检查记录](../evidence/reference-verification.txt)。

尝试用本机 RISC-V 工具链构建课程框架时，原始源码先因 `src/kernel/proc/type.h` 使用了未声明的 `spinlock_t` 而失败；该文件没有包含定义它的 `src/kernel/lock/type.h`。[原始构建输出](../evidence/course-framework-build.log)记录了错误。在 `/tmp` 临时副本加入这个 include 后，构建又因 `src/kernel/lib/print.c` 中未使用的 `printint`、`printptr` 函数被 `-Werror` 当成错误而失败，见[临时修补探测输出](../evidence/course-framework-patched-probe.log)。因此这个课程框架提交当前不能直接构建；我保留了未修改的课程原件，也清理了所有构建产物。正式实现时要先核对这些框架问题。

构建命令：`make -C course/lab6 -j2`。失败后已执行课程 `make clean`，没有把临时 `.o` 文件或 `target/` 构建产物留在归档里。

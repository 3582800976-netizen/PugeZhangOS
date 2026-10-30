# 课程依据与规划边界

核对日期：2026-10-30（Asia/Shanghai）。

核对目标是用户指定的 [ECNU OSLab 2026 Gitee 任务仓库](https://gitee.com/christinaaa/ecnu-oslab-2026-task)。本项目归档的课程原件包括 [Lab 0 总体课件](https://gitee.com/christinaaa/ecnu-oslab-2026-task/blob/master/lab-ppt/拔尖班-lab-0.pptx)、[Lab 4 课件](https://gitee.com/christinaaa/ecnu-oslab-2026-task/blob/master/lab-ppt/拔尖班-lab-4.pptx)、[Lab 5 课件](https://gitee.com/christinaaa/ecnu-oslab-2026-task/blob/master/lab-ppt/拔尖班-lab-5.pdf)。获取和校验过程见 [Lab 4 参考资料](../lab4/docs/reference/README.md)与 [Lab 5 参考资料](../lab5/docs/reference/README.md)。

2026-10-30 尝试搜索和打开上述 Gitee 仓库，但未能取得最新文件列表；因此本次不能确认是否发布了 Lab 6 新任务。这里记录的是访问受限，不代表仓库没有更新。后续继续以用户指定的 Gitee 地址为准。

本地目前可核对的详细课件止于 Lab 5；Lab 6–9 的新任务和详细要求尚未核实。本项目的候选目录和函数名不等于课程提供的源码框架。

| 课程阶段 | 总体内容 |
| --- | --- |
| 基础设施 | Lab 1 启动、Lab 2 内存、Lab 3 中断与异常 |
| 进程系统 | Lab 4 首个用户进程、Lab 5 用户堆与系统调用、Lab 6 调度与生命周期 |
| 文件系统 | Lab 7 磁盘和缓存、Lab 8 inode/目录项/路径、Lab 9 文件管理与 Shell |

课程要求连续地从零构建内核，参考 xv6-riscv-2020 的 util 分支。当前项目保留各阶段完整源码，用真实阶段接口衔接下一阶段。

Lab 4 第 4–6 页涉及 PCB、独立用户地址空间、内核 context 切换、U/S 往返和最小系统调用通路。因此不能把所有系统调用都推迟到 Lab 5，也不能把首次进程启动所需的 context 切换当作 Lab 6 才引入的机制；完整调用集合和多进程调度仍按后续实验发展。

Lab 5 第 3–5 页新增用户堆管理：进程保存空闲区链表表头，节点来自内核共享仓库，分配和释放处理区间分割、复用与合并。第 6–7 页梳理寄存器传参及系统调用往返，并将堆管理服务提供给用户程序。课件未规定完整调用集合与编号；本项目会扩展 Lab 4 已有通路，不把它当作尚未实现的能力。Lab 5 原件封面标注 2025 秋季，下载时保留原貌。

Lab 4 第 7–9 页要求 Spec Coding：本人理解系统与实验增量，完善模块 `functions` 的 `pre`、`effects`、`post`；AI 按规格实现，再逐条验收。先在规格设计完成后提交，再在代码生成完成后提交。下载课件或整理文档不算这两次设计与实现提交。本次查看的公开默认分支目录中尚未发现配套 YAML 规格，不能假定具体模块文件名已经发布。

课程 README 描述 Lab 1–3 先手动完成，之后引入 Agent 与 LLM。本项目现有 Lab 1–3 实现与自动检查由 Agent 协助完成，保留真实归属，不作为本人已独立手写的证明。此前列出的本人源码接手清单是项目建议，Lab 4 本人应写的规格契约需以新课件为准。

Lab 4 已实现 QEMU 单进程、独立地址空间、上下文与陷入往返、最小调用及保护检查。Lab 5 已在独立源码快照实现空闲区链表、动态堆页、分配/释放系统调用和堆状态展示；现有记录为 QEMU 三核通过，开发板运行尚未验证。两阶段规格均由 Agent 起草，本人契约解释仍待补充。Lab 6–9 未实现。本次检查课程默认分支根目录、`docs/` 和 `lab-ppt/`，仍未发现配套 YAML；不自行把项目草案当作课程原件。

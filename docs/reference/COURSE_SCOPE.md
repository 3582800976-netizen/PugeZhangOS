# 课程依据与规划边界

核对日期：2026-10-08（Asia/Shanghai）。

读取来源：[课程仓库 README](https://gitee.com/christinaaa/ecnu-oslab-2026-task)、[Lab 0 总体安排课件](https://gitee.com/christinaaa/ecnu-oslab-2026-task/blob/master/lab-ppt/拔尖班-lab-0.pptx)。通过公开 Gitee 内容 API 读取目录与 README，并解码 Lab 0 PPTX 提取第 6 页的阶段安排。

公开 `lab-ppt/` 当次只有 Lab 0、1、2、3 的详细课件。因此 Lab 4–9 的主题有课程依据，候选文件划分、接口和验收方法属于本项目的接续设计；未来详细课件发布后再确认。

| 课程阶段 | 总体内容 |
| --- | --- |
| 基础设施 | Lab 1 启动、Lab 2 内存、Lab 3 中断与异常 |
| 进程系统 | Lab 4 首个用户进程、Lab 5 系统调用与用户内存、Lab 6 调度与生命周期 |
| 文件系统 | Lab 7 磁盘和缓存、Lab 8 inode/目录项/路径、Lab 9 文件管理与 Shell |

课程要求连续地从零构建内核，参考 xv6-riscv-2020 的 util 分支。当前项目保留各阶段完整源码，用真实阶段接口衔接下一阶段。

原始请求是按课程建设系统；本轮额外要求是把实验和整体架构分清。此次只实现架构整理，未把后六个实验标为完成。

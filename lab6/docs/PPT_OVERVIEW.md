# Lab 6 PPT：八页讲什么

课件原件：[拔尖班-lab-6.pptx](../docs/reference/拔尖班-lab-6.pptx)。这份课件的主线是把 Lab 4/5 的单个 `proczero` 扩展成能调度、创建、等待、睡眠和退出的多个进程。

| 页 | 内容 | 换成直白的话 |
|---|---|---|
| 1 | 课程和实验标题 | 这是 2026 秋季从零实现小型内核的 Lab 6 |
| 2 | 本次四项主题 | 进程切换、fork/wait/sleep/exit、时钟调度和总结 |
| 3 | 内核上下文 `context` | 切换进程时，内核保存返回地址、栈指针和 `s0–s11` 等寄存器；切换只能由内核控制 |
| 4 | 调度器和 PCB 数组 | 创建进程与运行进程分开；只有 `RUNNABLE` 进程能被调度，调度器循环扫描进程表 |
| 5 | `fork` | 复制地址空间、堆链表和执行上下文，设置父子关系及子进程返回值；进程状态包括 UNUSED、RUNNABLE、RUNNING、SLEEPING、ZOMBIE |
| 6 | `wait`、`sleep`、`exit` | 父进程没有可回收的僵尸子进程时睡眠；子进程退出变成 ZOMBIE 并唤醒父进程；sleep 通过共享时钟 tick 决定唤醒时机 |
| 7 | 时钟抢占和测试 | 计算型进程不主动让出 CPU 时，时钟中断可强制调度；连续 fork 可产生多个进程测试调度 |
| 8 | 总结 | 复习 context 切换、fork 深拷贝、进程生命周期与时间片轮转 |

## 读课件时留意的边界

第 7 页说课件所依据的旧框架只能在用户态响应时钟，内核态抢占“后续修复”。当前 `lab6` 分支的 `spec/patches/lab6.yaml` 和 `spec/design.yaml` 已将内核态、用户态安全抢占列入目标。因此这句是在描述旧框架缺口，不代表最后验收可忽略内核态抢占。

课件没有展开完整的验收边界。进程复制失败、非法用户地址、资源回收、睡眠锁竞争、无丢失唤醒、1/2 hart 等细节，要继续看[课程指导书](../docs/reference/course/lab6/README.md)和[阶段规格](../docs/reference/course/lab6/spec/patches/lab6.yaml)。

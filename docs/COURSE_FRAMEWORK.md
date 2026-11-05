# 老师的课程框架与当前工作

用户确认实验要在老师的框架下填写。2026-11-04 从 [ECNU OSLab 课程仓库](https://gitee.com/christinaaa/ecnu-oslab-2026-task)取得 Lab 1～6 分支完整快照，逐个文件核对其 Git blob 哈希，与相应分支提交一致。以下 `course/` 保存老师的源码、指导书、许可证和其他文件；开始填写后，工作文件会与原始提交有所区别，原始版本可以按提交号重新取得。

| 实验 | 框架与指导书 | 取得的课程分支提交 | 初始文件数 |
| --- | --- | --- | --- |
| Lab 1 | [lab1/course](../lab1/course/README.md) | `e1f2efd5bcccf6a7f991192edc2ee8057844ed93` | 24 |
| Lab 2 | [lab2/course](../lab2/course/README.md) | `79b9ad32fca56697e8d904f349a151444e3fdcd8` | 38 |
| Lab 3 | [lab3/course](../lab3/course/README.md) | `00970e6549045bbc3a3f8bbacae09349f1d5ca36` | 42 |
| Lab 4 | [lab4/course](../lab4/course/README.md) | `11fd5f65bfc9fbc826a6388849aba9cbb3ed016a` | 66 |
| Lab 5 | [lab5/course](../lab5/course/README.md) | `4a0f4b72b17968d1430f32e40d8bef202ae544f4` | 89 |
| Lab 6 | [lab6/course](../lab6/course/README.md) | `7effc11729eaeb2231d49f0cdcf41619da5e9ca4` | 83 |

课程框架的主要代码在 `src/kernel/`；后续实验还有 `src/user/` 和 `spec/`。每个实验具体任务、允许直接沿用的文件和需要填写的内容，以相应指导书和规格为准。老师的后续分支还包含前阶段的空函数，因此必须把前阶段完成的工作按接口与课程更新正确衔接，不能只看目录就认为前面的功能已经完成。

原始 Lab 1 模板在临时副本中执行构建时，`print.c` 中 `printf` 尚未完成，未使用的辅助函数触发 `-Werror`；原始 Lab 4、5 模板也在相同位置构建失败。这是填写前的实际基线，不是实验完成后的验收结果。Lab 6 的历史基线见[参考资料记录](../lab6/docs/reference/README.md)。

在上述基线上，Lab 1 已在课程的 `boot/start.c`、`lib/print.c`、`lock/spinlock.c`、`main.c` 中填写启动、格式化和锁，并调整课程 Makefile 以限制双核及保证构建目录存在。QEMU 双核启动与并发检查见 [Lab 1 验证记录](../lab1/docs/evidence/course-2026-11-04/README.md)。

Lab 2 已按课程目录和接口衔接 Lab 1 的打印与锁，填写 OpenSBI 启动、物理页分配和内核页表。QEMU 双核启动、分配与映射等检查和限制见 [Lab 2 验证记录](../lab2/docs/evidence/course-2026-11-04/README.md)。

Lab 3 在老师框架中继承前阶段基础，填写时钟、外部中断分发、PLIC 与 UART 接收，修正课程模板中串口中断位定义和 trap 入口遗漏恢复 `tp` 的问题。双核时钟、代表性寄存器及串口交互检查见 [Lab 3 验证记录](../lab3/docs/evidence/course-2026-11-04/README.md)。目前测试覆盖有限，不能据此声称课程全部验收完成。

Lab 4～6 已在各自老师框架内填写并进行 QEMU 检查。Lab 4 覆盖用户态进入、异常隔离、权限和分配失败回滚，详见 [Lab 4 记录](../lab4/docs/evidence/course-2026-11-05/README.md)。Lab 5 覆盖用户内存、`brk`、`mmap`、栈增长和系统调用，详见 [Lab 5 记录](../lab5/docs/evidence/course-2026-11-05/README.md)。Lab 6 覆盖 `fork`、等待、调度、时钟抢占和锁，详见 [Lab 6 记录](../lab6/docs/evidence/course-2026-11-05/README.md)。测试只证明记录的场景；实板、全面压力测试和部分学生规格说明仍未完成。

Lab 4 的 `spec/modules/process.yaml` 和 `usertrap.yaml` 已由 Agent 起草；Lab 5、6 仍有学生需要填写和理解的规格占位内容，不能把它们记为本人完成。Lab 6 课程 `spec/design.yaml` 关于时钟计数核写的是 hart 0，而 `spec/modules/trap.yaml` 和增量规格写启动核；当前实现使用启动核，课程原件内部存在此差异，后续应向老师核实。

老师 Lab 1 指导书也要求学生在上阶段基础上建立实验分支。当前项目主要按目录保存实验；Git 分支尚未按老师要求重新整理，不能说已满足这一项。后续应结合课程要求整理连续历史，避免将新建立的分支冒充过去的提交记录。

此前由 Agent 协助制作的自建 Lab 1～5 代码及其运行记录保留在各实验原有位置，用于学习和按课程接口移植；它们没有在老师框架下验收，不能代替课程实验。课程框架实现的编译、QEMU 验收和实体板运行必须分别记录实际结果。

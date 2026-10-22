# Lab 5 规格与参与归属

本目录是本项目 Agent 根据课程 Lab 5 PDF 第 3–7 页起草的设计，不是课程提供的 YAML。2026-10-22 先提交规格，再生成 Lab 5 实现；沿用各 Lab 独立源码的结构。

本轮按“做 Lab 5”的授权推进实现。本人尚未解释或完善这些契约，不代填个人理解和操作记录。之后可以亲自运行 `heap`、`htest`，解释分配时为什么先保留节点、释放时为什么先检查整段、失败时怎样恢复资源。

阅读顺序：`design.yaml` → `modules/heap.yaml` / `modules/memory.yaml` / `modules/syscall.yaml` → `patches/lab5.yaml`。物理页、映射和元数据节点是不同资源。设计与源码的真实提交是两个阶段；下载课件不算设计完成。

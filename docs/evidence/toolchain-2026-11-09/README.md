# 用户程序布局检查的工具链维护

`scripts/check-course-user-layout.py` 检查 Lab 5、6 的链接脚本：只读代码能够链接，可写静态数据和 BSS 被拒绝。此前该脚本写死 `riscv64-linux-gnu-`，虽然课程 Makefile 也支持其他工具链前缀。

维护后，脚本按课程 Makefile 的候选前缀顺序寻找 assembler 和 linker；环境变量 `TOOLPREFIX` 可显式指定共同前缀。显式指定的工具链缺失时会报错并返回非零，不会悄悄换用其他工具链。

在仓库根目录运行：

```bash
python3 scripts/check-course-user-layout.py
python3 docs/evidence/toolchain-2026-11-09/recheck.py
```

[实际运行记录](layout-check.log)包含自动识别、用临时符号链接模拟自定义前缀，以及不存在的前缀三种情况。检查只验证工具选择和链接布局，不证明内核运行时行为。

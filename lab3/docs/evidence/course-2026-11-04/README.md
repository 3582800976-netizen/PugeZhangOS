# Lab 3 课程框架：2026-11-04 验证记录

工作目录是 `lab3/course/`，基于老师 Lab 3 提交 `00970e6549045bbc3a3f8bbacae09349f1d5ca36`。Agent 在课程框架中衔接前阶段启动、打印、锁和内存代码，并填写时钟、trap、PLIC、UART 等处理；同时修正课程模板串口接收中断位定义与 trap 返回遗漏恢复 `tp`。个人理解不冒充已经完成。

## 构建和启动

在课程目录执行 `make build`，见 [build.log](build.log)。使用 QEMU 的 OpenSBI、130 MiB、两个核心运行，正式目录输出见 [boot-runtime.log](boot-runtime.log)，两个核心都打印了 `ready with interrupts`。运行命令：

```bash
make -C lab3/course run
```

退出时先按 `Ctrl+A`，松开后再按 `X`。Lab 2 的跨核动态页表同步和空页表页回收限制在本阶段仍然存在。

## 时钟和串口测试

在课程工作目录的临时副本应用 [test-only.patch](test-only.patch) 后编译，见 [test-build.log](test-build.log)。补丁只在测试副本增加每核时钟中断计数、测试入口和寄存器探针。以 QEMU 双核启动后，两核各报告时钟中断及代表性寄存器恢复通过；向模拟串口发送 `helloZ`、退格、`!`、回车，得到相应字符、退格擦除序列和换行，见 [test-runtime.log](test-runtime.log)。发送完成后观察 1 秒没有额外输出；这次观察不足以证明不会发生所有中断风暴。

将另一个测试副本的 `src/kernel/trap/type.h` 中 `INTERVAL` 从 1,000,000 临时改为 200,000，其余测试相同，构建和输出见 [fast-build.log](fast-build.log)、[fast-runtime.log](fast-runtime.log)。本次每核观察到的中断次数由 2 次增加到 14 次；调度与时间测量存在变化，这是一组实验现象，不是精确频率保证。

测试结束由脚本或 `timeout` 发送 SIGTERM；这不是内核自主关机。测试只检查了部分代表性寄存器、有限的串口输入和双核 QEMU，尚未覆盖所有异常、持续压力或开发板；不能据此声称全部课程验收通过。

后续重跑、构建修复及 GDB 入口验证见[复查记录](../recheck-2026-11-04/README.md)。

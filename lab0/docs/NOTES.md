# 本人的实验记录

本文件留给张璞格填写。以下空白不是已完成成果，Agent 不代填理解和操作经历。

## Lab 0：亲手运行参考 xv6

- 我执行的命令：make qemu
  看到xv6 的$ echo hello-xv6 
                ls
- 我看到的结果：

zpg@LAPTOP-JVDAI2F6:~/Work/xv6-labs-2020$ make qemu
qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

xv6 kernel is booting

hart 2 starting
hart 1 starting
init: starting sh
$ echo hello-xv6
hello-xv6
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2059
xargstest.sh   2 3 93
cat            2 4 21104
echo           2 5 20048
forktest       2 6 12096
grep           2 7 23600
init           2 8 20912
kill           2 9 19936
ln             2 10 19848
ls             2 11 23312
mkdir          2 12 20024
rm             2 13 20016
sh             2 14 36024
stressfs       2 15 21032
usertests      2 16 129352
grind          2 17 33904
wc             2 18 21784
zombie         2 19 19456
console        3 20 0
$ 

- GCC、QEMU、xv6 各负责什么：GCC 把 C 和汇编源码编译成 RISC-V 机器能执行的程序；QEMU 模拟 RISC-V 的 CPU、内存和设备；xv6 是运行在这台模拟机器里的操作系统，负责管理资源并运行用户程序。
- Shell 的 `$` 属于哪里：这里的 $ 是 xv6 内部 Shell 的命令提示符，表示正在等待输入命令。输入的 echo 和 ls 都在 xv6 中执行，不是在宿主 Linux 中执行。


## 每一步可重复使用的模板

### 实验步骤与目标

### 操作前，我预测会发生什么

### 我参考了 xv6 的哪些设计

### 我决定的接口、数据结构及理由

### 我自己完成的代码或修改

### Agent 提供了哪些帮助

### 我亲手运行的命令、结果及异常

### 我的解释：为什么会出现这个结果

### 下一次只改变一个条件，我预测会怎样

### 真实提交记录

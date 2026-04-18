# nohup-lite (aarch64 linux)
抛弃libc的nohup，用于大量缩减可执行文件容量。
大部分由ai编写。

## 如果真要用
entry.S - 用于处理程序入参和环境变量，遵循aarch64 abi  
main.c - 主逻辑，写满系统调用，main实现主要逻辑  
参考[build.sh](build.sh)  
[origin.c](https://github.com/mirror/busybox/blob/1_36_1/coreutils/nohup.c) - busybox nohup的实现

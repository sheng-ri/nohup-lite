CC=/opt/cross/bin/aarch64-linux-musl-gcc
$CC -nostdlib -fno-builtin -static -O2 -o nohup main.c entry.S 

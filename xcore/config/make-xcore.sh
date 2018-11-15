#!/bin/bash
dd if=/dev/zero of=xcore.img bs=1M count=100

fdisk xcore.img << EOF
x
c
203
h
16
s
63
r
n
p
1
1
203
a
1
w
EOF

losetup -o 32256 /dev/loop0 xcore.img

mke2fs /dev/loop0
mount /dev/loop0 kmnt/
cd kmnt
mkdir -p boot/grub
cd boot
cp /usr/local/winshare/kernel .
cp /usr/local/winshare/initrd.img initrd
cd grub
cp /boot/grub/stage1 . -r
cp /boot/grub/stage2 . -r
cp /boot/grub/e2fs_stage1_5 . -r
cd /usr/local
umount kmnt/

grub --device-map=/dev/null << EOF
device (hd0) xcore.img
geometry (hd0) 203 16 63
root (hd0,0)
setup (hd0)
quit
EOF

losetup -d /dev/loop0
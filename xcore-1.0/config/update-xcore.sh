#!/bin/bash
cd /usr/local/winshare/xcore
make clean
make all
cd /usr/local
losetup -o 32256 /dev/loop0 xcore.img
mount /dev/loop0 kmnt/
cd kmnt/boot
cp /usr/local/winshare/xcore/build/kernel . -f
cd /usr/local
umount kmnt/
losetup -d /dev/loop0
cp xcore.img /usr/local/winshare/ -f

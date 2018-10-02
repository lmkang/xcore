BUILD_DIR = build
ENTRY_ADDR = 0xc0001500
AS = nasm
CC = gcc
LD = ld
INCLUDES = -I lib/ -I lib/kernel/ -I kernel/ -I device/ -I thread/ -I user/ -I lib/user/
ASFLAGS = -f elf
CFLAGS = $(INCLUDES) -m32 -Wall -W -std=c99 -fno-builtin
LDFLAGS = -melf_i386 -Ttext $(ENTRY_ADDR) -e main -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o \
$(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/string.o \
$(BUILD_DIR)/memory.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/switch.o \
$(BUILD_DIR)/console.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/list.o \
$(BUILD_DIR)/debug.o $(BUILD_DIR)/print.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/ioqueue.o \
$(BUILD_DIR)/tss.o $(BUILD_DIR)/process.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/uproc.o \
$(BUILD_DIR)/stdio.o
TARGET_NAME = xcore.img

################### C代码编译 ################
$(BUILD_DIR)/main.o : kernel/main.c lib/kernel/print.h lib/stdint.h \
kernel/init.h kernel/memory.h lib/string.h kernel/debug.h thread/thread.h \
device/console.h device/ioqueue.h device/keyboard.h user/process.h \
lib/user/unistd.h lib/stdio.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/init.o : kernel/init.c lib/kernel/print.h \
lib/stdint.h kernel/interrupt.h device/timer.h device/console.h \
device/timer.h thread/thread.h user/tss.h device/keyboard.h \
device/keyboard.h lib/kernel/syscall.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/interrupt.o : kernel/interrupt.c lib/stdint.h \
kernel/global.h lib/kernel/x86.h lib/kernel/print.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/timer.o : device/timer.c lib/stdint.h \
lib/kernel/x86.h lib/kernel/print.h kernel/debug.h kernel/interrupt.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/debug.o : kernel/debug.c lib/kernel/print.h \
lib/stdint.h kernel/interrupt.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/string.o : lib/string.c lib/stdint.h kernel/debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/bitmap.o : lib/kernel/bitmap.c lib/stdint.h \
kernel/global.h lib/string.h kernel/debug.h lib/kernel/bitmap.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/memory.o : kernel/memory.c lib/stdint.h lib/kernel/print.h \
kernel/memory.h lib/kernel/bitmap.h kernel/global.h lib/string.h kernel/debug.h \
thread/sync.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/thread.o : thread/thread.c lib/stdint.h lib/string.h kernel/global.h \
kernel/memory.h thread/thread.h lib/kernel/list.h thread/sync.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/list.o : lib/kernel/list.c lib/kernel/list.h kernel/interrupt.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/sync.o : thread/sync.c thread/sync.h lib/stdint.h lib/kernel/list.h \
kernel/interrupt.h thread/thread.h kernel/debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/console.o : device/console.c thread/sync.h lib/stdint.h lib/kernel/print.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/keyboard.o : device/keyboard.c lib/kernel/print.h kernel/interrupt.h \
kernel/global.h lib/kernel/x86.h device/keyboard.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/ioqueue.o : device/ioqueue.c device/ioqueue.h kernel/interrupt.h \
kernel/global.h kernel/debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/tss.o : user/tss.c lib/stdint.h thread/thread.h kernel/global.h \
lib/kernel/print.h lib/string.h kernel/memory.h lib/string.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/process.o : user/process.c user/process.h lib/stdint.h kernel/interrupt.h \
lib/string.h kernel/memory.h kernel/global.h lib/kernel/bitmap.h lib/kernel/list.h \
kernel/debug.h user/tss.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/syscall.o : lib/kernel/syscall.c lib/kernel/syscall.h lib/stdint.h thread/thread.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/uproc.o : lib/user/uproc.c lib/kernel/syscall.h lib/stdint.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/stdio.o : lib/stdio.c lib/stdint.h lib/stdio.h kernel/global.h lib/string.h \
lib/user/unistd.h
	$(CC) $(CFLAGS) -c -o $@ $<

################### 汇编代码编译 ################
$(BUILD_DIR)/mbr.bin : boot/mbr.S boot/loader.inc
	$(AS) -I boot/ -o $@ $<

$(BUILD_DIR)/loader.bin : boot/loader.S boot/loader.inc boot/boot.inc
	$(AS) -I boot/ -o $@ $<

$(BUILD_DIR)/print.o : lib/kernel/print.S
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/kernel.o : kernel/kernel.S
	$(AS) $(ASFLAGS) -o $@ $<

$(BUILD_DIR)/switch.o : thread/switch.S
	$(AS) $(ASFLAGS) -o $@ $<

################### 链接所有文件 ################	
$(BUILD_DIR)/kernel.bin : $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

.PHONY : makedir hd clean all

makedir : 
	if [[ ! -d $(BUILD_DIR) ]];then mkdir $(BUILD_DIR);fi

hd : $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin $(BUILD_DIR)/kernel.bin
	dd if=/dev/zero of=$(BUILD_DIR)/$(TARGET_NAME) bs=1M count=10
	dd if=$(BUILD_DIR)/mbr.bin of=$(BUILD_DIR)/$(TARGET_NAME) bs=512 count=1 conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=$(BUILD_DIR)/$(TARGET_NAME) bs=512 count=4 seek=1 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=$(BUILD_DIR)/$(TARGET_NAME) bs=512 count=200 seek=5 conv=notrunc
clean : 
	rm $(BUILD_DIR)/*

all : makedir hd
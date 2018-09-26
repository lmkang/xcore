BUILD_DIR = build
ENTRY_ADDR = 0xc0001500
AS = nasm
CC = gcc
LD = ld
INCLUDES = -I lib/ -I lib/kernel/ -I kernel/ -I device/
ASFLAGS = -f elf
CFLAGS = $(INCLUDES) -m32 -Wall -W -std=c99 -fno-builtin
LDFLAGS = -melf_i386 -Ttext $(ENTRY_ADDR) -e main -Map $(BUILD_DIR)/kernel.map
OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o \
$(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/string.o \
$(BUILD_DIR)/memory.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/debug.o $(BUILD_DIR)/print.o
TARGET_NAME = xcore.img

################### C代码编译 ################
$(BUILD_DIR)/main.o : kernel/main.c lib/kernel/print.h lib/stdint.h \
kernel/init.h kernel/memory.h lib/string.h kernel/debug.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/init.o : kernel/init.c lib/kernel/print.h \
lib/stdint.h kernel/interrupt.h device/timer.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/interrupt.o : kernel/interrupt.c lib/stdint.h \
kernel/global.h lib/kernel/x86.h lib/kernel/print.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/timer.o : device/timer.c lib/stdint.h \
lib/kernel/x86.h lib/kernel/print.h
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
kernel/memory.h lib/kernel/bitmap.h kernel/global.h lib/string.h kernel/debug.h
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
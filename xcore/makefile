BUILD_DIR = build
AS = nasm
CC = gcc
LD = ld
AS_FLAGS = -f elf
C_FLAGS = -m32 -Wall -W -std=c99 -nostdinc -fno-builtin -fno-stack-protector
LD_FLAGS = -melf_i386 -nostdlib -Tlink.ld
INCLUDES = 
OBJS = $(BUILD_DIR)/boot.o $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o \
	$(BUILD_DIR)/descriptor.o $(BUILD_DIR)/print.o $(BUILD_DIR)/putchar.o \
	$(BUILD_DIR)/interrupt.o $(BUILD_DIR)/idt.o $(BUILD_DIR)/debug.o \
	$(BUILD_DIR)/string.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/bitmap.o \
	$(BUILD_DIR)/timer.o $(BUILD_DIR)/thread.o $(BUILD_DIR)/switch.o \
	$(BUILD_DIR)/list.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/keyboard.o \
	$(BUILD_DIR)/ioqueue.o $(BUILD_DIR)/process.o $(BUILD_DIR)/syscall.o \
	$(BUILD_DIR)/stdio.o $(BUILD_DIR)/ide.o $(BUILD_DIR)/fs.o $(BUILD_DIR)/inode.o \
	$(BUILD_DIR)/file.o $(BUILD_DIR)/directory.o $(BUILD_DIR)/fork.o
TARGET_NAME = kernel

$(BUILD_DIR)/%.o : %.c
	$(CC) $(C_FLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o : %.S
	$(AS) $(AS_FLAGS) -o $@ $<

$(BUILD_DIR)/$(TARGET_NAME) : $(OBJS)
	$(LD) $(LD_FLAGS) -o $@ $^

.PHONY : makedir build all clean

makedir : 
	if [[ ! -d $(BUILD_DIR) ]];then mkdir $(BUILD_DIR);fi

build : $(BUILD_DIR)/$(TARGET_NAME)

all : makedir build

clean : 
	rm $(BUILD_DIR)/*
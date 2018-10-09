BUILD_DIR = build
AS = nasm
CC = gcc
LD = ld
AS_FLAGS = -f elf
C_FLAGS = -m32 -Wall -W -std=c99 -nostdinc -fno-builtin -fno-stack-protector
LD_FLAGS = -melf_i386 -nostdlib -Tlink.ld
INCLUDES = 
OBJS = $(BUILD_DIR)/boot.o $(BUILD_DIR)/main.o $(BUILD_DIR)/print.o \
	$(BUILD_DIR)/gdt.o $(BUILD_DIR)/init.o $(BUILD_DIR)/gdtasm.o \
	$(BUILD_DIR)/interrupt.o $(BUILD_DIR)/intrasm.o
TARGET_NAME = kernel

$(BUILD_DIR)/main.o : main.c
	$(CC) $(C_FLAGS) -c -o $@ $<

$(BUILD_DIR)/boot.o : boot.S
	$(AS) $(AS_FLAGS) -o $@ $<

$(BUILD_DIR)/print.o : print.S
	$(AS) $(AS_FLAGS) -o $@ $<

$(BUILD_DIR)/gdt.o : gdt.c
	$(CC) $(C_FLAGS) -c -o $@ $<

$(BUILD_DIR)/init.o : init.c
	$(CC) $(C_FLAGS) -c -o $@ $<

$(BUILD_DIR)/gdtasm.o : gdtasm.S
	$(AS) $(AS_FLAGS) -o $@ $<

$(BUILD_DIR)/interrupt.o : interrupt.c
	$(CC) $(C_FLAGS) -c -o $@ $<

$(BUILD_DIR)/intrasm.o : intrasm.S
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
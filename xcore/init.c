#include "gdt.h"
#include "interrupt.h"
#include "memory.h"

void init_all() {
	init_gdt(); // 初始化GDT
	init_idt(); // 初始化IDT
	init_kernel_vmm(); // 初始化内核页目录和页表
}
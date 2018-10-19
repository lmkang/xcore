#include "gdt.h"
#include "interrupt.h"
#include "memory.h"

void init_all(uint32_t mem_size) {
	init_gdt(); // 初始化GDT
	init_idt(); // 初始化IDT
	init_timer(); // 初始化定时器
	init_kernel_vmm(); // 初始化内核页目录和页表
	init_mem_pool(mem_size); // 初始化内存管理
}
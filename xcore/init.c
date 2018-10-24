#include "descriptor.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "timer.h"
#include "print.h"
#include "keyboard.h"
#include "global.h"

void init_all(uint32_t mem_size) {
	init_gdt(); // 初始化GDT
	init_idt(); // 初始化IDT
	init_timer(); // 初始化定时器
	init_kernel_vmm(); // 初始化内核页目录和页表
	init_mem_pool(*((uint32_t*) P2V(TOTAL_MEM_SIZE_PADDR))); // 初始化内存管理
	thread_init(); // 初始化线程
	console_init(); //初始化终端
	keyboard_init(); // 初始化键盘
}
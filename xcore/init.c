#include "descriptor.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "timer.h"
#include "print.h"
#include "keyboard.h"
#include "syscall.h"

void init_all() {
	init_gdt(); // 初始化GDT
	init_idt(); // 初始化IDT
	init_timer(); // 初始化定时器
	mm_init(); // 初始化内存管理
	thread_init(); // 初始化线程
	console_init(); //初始化终端
	keyboard_init(); // 初始化键盘
	syscall_init(); // 初始化系统调用
}
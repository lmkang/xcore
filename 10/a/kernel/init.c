#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall.h"
#include "ide.h"
#include "fs.h"

// 初始化所有模块
void init_all() {
	put_str("init_all\n");
	init_idt(); // 初始化中断
	init_memory(); // 初始化内存管理系统
	thread_init(); // 初始化线程相关结构
	init_timer(); // 初始化PIT
	console_init(); // 初始化控制台,最好放在开中断之前
	keyboard_init(); // 初始化键盘
	tss_init(); // 初始化TSS
	syscall_init(); // 初始化系统调用
	ide_init(); // 初始化硬盘
	fs_init(); // 初始化文件系统
}
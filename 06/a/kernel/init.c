#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"

// 初始化所有模块
void init_all() {
	put_str("init_all\n");
	init_idt();
	init_memory();
	thread_init();
	init_timer();
	
}
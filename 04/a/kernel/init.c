#include "print.h"
#include "interrupt.h"

// 初始化所有模块
void init_all() {
	put_str("init_all\n");
	init_idt();
}
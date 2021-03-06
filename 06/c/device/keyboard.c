#include "print.h"
#include "interrupt.h"
#include "global.h"
#include "x86.h"

#define KEYBOARD_BUF_PORT 0x60 // 键盘buffer寄存器端口号

// 键盘中断处理程序
static void intr_keyboard_handler(void) {
	put_char('k');
	// 必须要读取输出缓冲区寄存器,否则8042不再继续响应键盘中断
	inb(KEYBOARD_BUF_PORT);
}

// 键盘初始化
void keyboard_init(void) {
	put_str("keyboard_init start\n");
	register_handler(0x21, intr_keyboard_handler);
	put_str("keyboard_init done\n");
}
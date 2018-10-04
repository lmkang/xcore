#include "print.h"
#include "sync.h"
#include "ulib.h"

static struct lock console_lock; // 控制台锁

// 初始化终端
void console_init() {
	lock_init(&console_lock);
}

// 获取终端
void console_acquire() {
	lock_acquire(&console_lock);
}

// 释放终端
void console_release() {
	lock_release(&console_lock);
}

// 终端中输出字符串
void console_put_str(char *str) {
	console_acquire();
	put_str(str);
	console_release();
}

// 终端中输出字符
void console_put_char(uint8_t ch) {
	console_acquire();
	put_char(ch);
	console_release();
}

// 终端中输出十六进制整数
void console_put_int(uint32_t i) {
	console_acquire();
	put_int(i);
	console_release();
}

// 供内核使用的格式化输出函数
void printk(const char *format, ...) {
	va_list args;
	va_start(args, format);
	char buf[1024] = {0};
	vsprintf(buf, format, args);
	va_end(args);
	console_put_str(buf);
}
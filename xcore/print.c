#include "x86.h"
#include "string.h"
#include "sync.h"
#include "stdio.h"

extern void put_char(uint8_t ch);

static struct lock console_lock; // 终端锁

// 设置光标位置
void set_cursor(uint16_t cursor_pos) {
	outb(0x03d4, 0x0e);
	outb(0x03d5, ((cursor_pos >> 8) & 0x00ff));
	outb(0x03d4, 0x0f);
	outb(0x03d5, (cursor_pos & 0x00ff));
}

// 获取光标位置
uint16_t get_cursor() {
	uint8_t cursor_high = 0; // 光标高8位
	uint8_t cursor_low = 0; // 光标低8位
	outb(0x03d4, 0x0e);
	cursor_high = inb(0x03d5);
	outb(0x03d4, 0x0f);
	cursor_low = inb(0x03d5);
	return (cursor_high << 8) | cursor_low;
}

// 输出一个字符串
void put_str(char *str) {
	char *p = str;
	while(*p != '\0') {
		put_char(*p++);
	}
}

// 以16进制输出整型,包含前缀0x
void put_hex(uint32_t i) {
	if((i & 0xffffffff) == 0) {
		put_str("0x0");
	} else {
		put_str("0x");
		bool prefix_zero = false; // 标记是否有前缀0
		if(((i >> 28) & 0xf) == 0) {
			prefix_zero = true;
		}
		for(int idx = 28; idx >= 0; idx -= 4) {
			int val = ((i >> idx) & 0xf);
			if(val != 0) {
				prefix_zero = false;
			}
			if(!prefix_zero) {
				if(val < 10) {
					put_char('0' + val);
				} else {
					put_char('a' + (val - 10));
				}
			}
		}
	}
}

// 格式化输出字符串
uint32_t printk(const char *format, ...) {
	va_list args;
	va_start(args, format); // 将args指向format
	char buf[1024] = {0}; // 用于存储拼接后的字符串
	uint32_t ret_val = vsprintf(buf, format, args);
	va_end(args);
	put_str(buf);
	return ret_val;
}

// 初始化终端
void console_init() {
	lock_init(&console_lock);
}

// 终端输出字符
void console_put_char(char ch) {
	lock_acquire(&console_lock);
	put_char(ch);
	lock_release(&console_lock);
}

// 终端输出字符串
void console_put_str(char *str) {
	lock_acquire(&console_lock);
	put_str(str);
	lock_release(&console_lock);
}

// 终端格式化输出
uint32_t console_printk(const char *format, ...) {
	lock_acquire(&console_lock);
	va_list args;
	va_start(args, format); // 将args指向format
	char buf[1024] = {0}; // 用于存储拼接后的字符串
	uint32_t ret_val = vsprintf(buf, format, args);
	va_end(args);
	put_str(buf);
	lock_release(&console_lock);
	return ret_val;
}
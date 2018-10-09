#include "x86.h"

void put_char(uint8_t ch);

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
#include "x86.h"

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

static void scroll(uint16_t cursor_pos, uint16_t *video_ptr) {
	// 超出最后一行一列
	if(cursor_pos >= 80 * 25) {
		// 1-24行 --> 0-23行
		for(uint16_t i = 1 * 80; i < 24 * 80; i++) {
			*(video_ptr + i - 80) = *(video_ptr + i);
		}
		// 清空24行
		for(uint16_t i = 24 * 80; i < 25 * 80; i++) {
			*(video_ptr + i) = (0x0f << 8) | 0x20; // 0x20:space
		}
		// 设置光标位置为24行行首
		set_cursor(24 * 80);
	} else {
		// 设置光标位置
		set_cursor(cursor_pos);
	}
}

// 输出一个字符
void put_char(uint8_t ch) {
	uint16_t *video_ptr = (uint16_t*) 0xb8000;
	uint16_t cursor_pos = get_cursor(); // 获取光标位置
	// 字符处理,bgcolor:black,fgcolor:white
	if(ch == '\n') { // carriage return
		// 滚动屏幕,光标换到下一行行首
		scroll((cursor_pos / 80 + 1) * 80, video_ptr);
	} else if(ch == '\t') { // tab
		// 显示4个空格
		for(uint8_t i = 1; i <= 4; i++) {
			video_ptr += (cursor_pos + i);
			*video_ptr = (0x0f << 8) | 0x20;
			// 滚动屏幕
			scroll(cursor_pos + i, video_ptr);
		}
	} else if(ch == '\b') { // backspace
		// 光标前移,用空格覆盖
		video_ptr += (cursor_pos - 1);
		*video_ptr = (0x0f << 8) | 0x20;
	} else {
		// 显示字符
		video_ptr += cursor_pos;
		*video_ptr = (0x0f << 8) | ch;
		// 滚动屏幕
		scroll(cursor_pos + 1, video_ptr);
	}
}

// 输出一个字符串
void put_str(char *str) {
	char *p = str;
	while(*p != '\0') {
		put_char(*p);
		++p;
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
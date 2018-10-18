#include "x86.h"
#include "string.h"

typedef char* va_list;

#define va_start(ap, v) ap = (va_list) &v
#define va_arg(ap, t) *((t*) (ap += 4))
#define va_end(ap) ap = NULL

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

// 将整型转换成字符
static void itoa(uint32_t value, char **buf, uint8_t base) {
	uint32_t m = value % base; // 求模,最低位
	uint32_t i = value / base; // 取整
	if(i) { // 如果倍数不为0,则递归调用
		itoa(i, buf, base);
	}
	if(m < 10) { // 如果余数是0~9
		*((*buf)++) = m + '0';
	} else {
		*((*buf)++) = m - 10 + 'A';
	}
}

// 将参数ap按照格式format输出到字符串str,并返回替换后str长度
uint32_t vsprintf(char *str, const char *format, va_list ap) {
	char *buf = str;
	const char *idx_ptr = format;
	char idx_ch = *idx_ptr;
	int32_t arg_int;
	char *arg_str;
	while(idx_ch) {
		if(idx_ch != '%') {
			*(buf++) = idx_ch;
			idx_ch = *(++idx_ptr); // 得到%后面的字符
			continue;
		}
		idx_ch = *(++idx_ptr);
		switch(idx_ch) {
			case 's':
				arg_str = va_arg(ap, char*);
				strcpy(buf, arg_str);
				buf += strlen(arg_str);
				idx_ch = *(++idx_ptr);
				break;
			case 'c':
				*(buf++) = va_arg(ap, char);
				idx_ch = *(++idx_ptr);
				break;
			case 'd':
				arg_int = va_arg(ap, int);
				// 若是负数,将其转为正数后,在正数前面输出个负号'-'
				if(arg_int < 0) {
					arg_int = 0 - arg_int;
					*buf++ = '-';
				}
				itoa(arg_int, &buf, 10);
				idx_ch = *(++idx_ptr);
				break;
			case 'x':
				arg_int = va_arg(ap, int);
				// 16进制加上前缀0x
				*buf++ = '0';
				*buf++ = 'x';
				itoa(arg_int, &buf, 16);
				idx_ch = *(++idx_ptr);
				// 跳过格式字符并更新idx_ch
				break;
		}
	}
	return strlen(str);
}

// 同printf不同的地方就是字符串不是写到终端,而是写到buf中
uint32_t sprintf(char *buf, const char *format, ...) {
	va_list args;
	uint32_t ret_val;
	va_start(args, format);
	ret_val = vsprintf(buf, format, args);
	va_end(args);
	return ret_val;
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
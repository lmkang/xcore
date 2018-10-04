#include "stdint.h"
#include "syscall.h"
#include "ulib.h"
#include "global.h"
#include "string.h"

#define va_start(ap, v) ap = (va_list) &v
#define va_arg(ap, t) *((t*) (ap += 4))
#define va_end(ap) ap = NULL

// 获取进程pid
uint32_t getpid() {
	return _syscall0(SYS_GETPID);
}

// 打印字符串
uint32_t write(char *str) {
	return _syscall1(SYS_WRITE, str);
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
uint32_t printf(const char *format, ...) {
	va_list args;
	va_start(args, format); // 将args指向format
	char buf[1024] = {0}; // 用于存储拼接后的字符串
	vsprintf(buf, format, args);
	va_end(args);
	return write(buf);
}

// 申请size字节大小的内存,并返回结果
void * malloc(uint32_t size) {
	return (void*) _syscall1(SYS_MALLOC, size);
}

// 释放ptr指向的内存
void free(void *ptr) {
	_syscall1(SYS_FREE, ptr);
}
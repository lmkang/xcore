#include "syscall.h"

// 获取进程pid
uint32_t getpid() {
	return _syscall0(SYS_GETPID);
}

// 打印字符串
uint32_t write(char *str) {
	return _syscall1(SYS_WRITE, str);
}
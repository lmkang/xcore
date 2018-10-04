#include "syscall.h"
#include "thread.h"
#include "print.h"
#include "console.h"
#include "string.h"

#define SYSCALL_COUNT 32
void* syscall_table[SYSCALL_COUNT];

// 初始化系统调用
void syscall_init(void) {
	put_str("syscall_init start\n");
	syscall_table[SYS_GETPID] = sys_getpid;
	syscall_table[SYS_WRITE] = sys_write;
	syscall_table[SYS_MALLOC] = sys_malloc;
	syscall_table[SYS_FREE] = sys_free;
	
	put_str("syscall_init done\n");
}

// 返回当前任务的pid
uint32_t sys_getpid(void) {
	return current_thread()->pid;
}

// 打印字符串str(未实现文件系统前的版本)
uint32_t sys_write(char *str) {
	console_put_str(str);
	return strlen(str);
}
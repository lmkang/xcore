#include "types.h"
#include "thread.h"
#include "print.h"
#include "syscall.h"
#include "string.h"

#define SYSCALL_COUNT 32

void *syscall_table[SYSCALL_COUNT];

// 无参数的系统调用
#define _syscall0(NUMBER) ({ \
	int value; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(value) \
		: "a"(NUMBER) \
		: "memory" \
	); \
	value; \
})

// 一个参数的系统调用
#define _syscall1(NUMBER, ARG1) ({ \
	int value; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(value) \
		: "a"(NUMBER), "b"(ARG1) \
		: "memory" \
	); \
	value; \
})

// 两个参数的系统调用
#define _syscall2(NUMBER, ARG1, ARG2) ({ \
	int value; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(value) \
		: "a"(NUMBER), "b"(ARG1), "c"(ARG2) \
		: "memory" \
	); \
	value; \
})

// 三个参数的系统调用
#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({ \
	int value; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(value) \
		: "a"(NUMBER), "b"(ARG1), "c"(ARG2), "d"(ARG3) \
		: "memory" \
	); \
	value; \
})

// 返回当前任务的pid
pid_t sys_getpid(void) {
	return current_thread()->pid;
}

// 打印字符串
uint32_t sys_write(char *str) {
	console_put_str(str);
	return strlen(str);
}

// 初始化系统调用
void syscall_init(void) {
	syscall_table[SYS_GETPID] = sys_getpid;
	syscall_table[SYS_WRITE] = sys_write;
	
	printk("syscall_init done\n");
}

// -------- user program call ------------

// 返回当前任务pid
pid_t getpid(void) {
	return _syscall0(SYS_GETPID);
}

// 打印字符串
uint32_t write(char *str) {
	return _syscall1(SYS_WRITE, str);
}
































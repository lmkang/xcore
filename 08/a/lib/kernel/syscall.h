#ifndef __LIB_KERNEL_SYSCALL_H
#define __LIB_KERNEL_SYSCALL_H
#include "stdint.h"

// 无参数的系统调用
#define _syscall0(NUMBER) ({ \
	int ret_val; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(ret_val) \
		: "a"(NUMBER) \
		: "memory" \
	); \
	ret_val; \
})

// 1个参数的系统调用
#define _syscall1(NUMBER, ARG1) ({ \
	int ret_val; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(ret_val) \
		: "a"(NUMBER), "b"(ARG1) \
		: "memory" \
	); \
	ret_val; \
})

// 2个参数的系统调用
#define _syscall2(NUMBER, ARG1, ARG2) ({ \
	int ret_val; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(ret_val) \
		: "a"(NUMBER), "b"(ARG1), "c"(ARG2) \
		: "memory" \
	); \
	ret_val; \
})

// 3个参数的系统调用
#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({ \
	int ret_val; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(ret_val) \
		: "a"(NUMBER), "b"(ARG1), "c"(ARG2), "d"(ARG3) \
		: "memory" \
	); \
	ret_val; \
})

enum SYSCALL_COUNT {
	SYS_GETPID
};

uint32_t sys_getpid(void);

void syscall_init(void);

#endif
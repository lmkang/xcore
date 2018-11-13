#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "types.h"

enum SYSCALL_NR {
	SYS_GETPID,
	SYS_WRITE,
	SYS_MALLOC,
	SYS_FREE,
	SYS_FORK,
	SYS_PUTCHAR,
	SYS_CLEAR
};

// ----- user call ----------

pid_t getpid(void);

int32_t write(int32_t fd, const void *buf, uint32_t count);

void *malloc(uint32_t size);

void free(void *ptr);

void putchar(char ch);

void clear(void);

// ----- kernel call --------

void syscall_init(void);

pid_t sys_getpid(void);

#endif
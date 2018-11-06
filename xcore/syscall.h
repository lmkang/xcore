#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "types.h"

enum SYSCALL_NR {
	SYS_GETPID,
	SYS_WRITE,
	SYS_MALLOC,
	SYS_FREE,
	SYS_FORK,
	SYS_READ,
	SYS_PUTCHAR,
	SYS_CLEAR,
	SYS_GETCWD,
	SYS_OPEN,
	SYS_CLOSE,
	SYS_LSEEK,
	SYS_UNLINK,
	SYS_MKDIR,
	SYS_OPENDIR,
	SYS_CLOSEDIR,
	SYS_CHDIR,
	SYS_RMDIR,
	SYS_READDIR,
	SYS_REWINDDIR,
	SYS_STAT,
	SYS_PS
};

// ----- user call ----------

pid_t getpid(void);

uint32_t write(int32_t fd, const void *buf, uint32_t count);

void *malloc(uint32_t size);

void free(void *ptr);

int32_t read(int32_t fd, void *buf, uint32_t count);

void putchar(char ch);

void clear(void);

// ----- kernel call --------

void syscall_init(void);

pid_t sys_getpid(void);

#endif
#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "types.h"

enum SYSCALL_NR {
	SYS_GETPID,
	SYS_WRITE
};

// ----- user call ----------

pid_t getpid(void);

uint32_t write(char *str);

// ----- kernel call --------

void syscall_init(void);

pid_t sys_getpid(void);

uint32_t sys_write(char *str);

#endif
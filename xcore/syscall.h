#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "types.h"

enum SYSCALL_NR {
	SYS_GETPID
};

// ----- user call ----------

pid_t getpid(void);

// ----- kernel call --------

void syscall_init(void);

pid_t sys_getpid(void);

#endif
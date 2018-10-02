#include "syscall.h"

uint32_t getpid() {
	return _syscall0(SYS_GETPID);
}
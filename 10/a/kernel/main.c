#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "memory.h"
#include "ulib.h"

int main() {
	put_str("hello,kernel!\n");
	init_all();
	
	enable_intr();// 打开中断,使时钟中断起作用
	
	while(1);
	return 0;
}
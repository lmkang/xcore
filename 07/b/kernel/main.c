#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "process.h"

void k_thread_a(void*);

void k_thread_b(void*);

void u_prog_a(void);

void u_prog_b(void);

int var_a = 0, var_b = 0;

int main() {
	put_str("hello,kernel!\n");
	init_all();
	thread_start("k_thread_a", 31, k_thread_a, "argA ");
	thread_start("k_thread_b", 8, k_thread_b, "argB ");
	process_execute(u_prog_a, "user_prog_a");
	process_execute(u_prog_b, "user_prog_b");
	enable_intr();// 打开中断,使时钟中断起作用
	while(1) {
		console_put_str("Main ");
	}
	return 0;
}

void k_thread_a(void *arg) {
	while(1) {
		console_put_str("var_a:0x");
		console_put_int(var_a);
		console_put_str(" ");
	}
}

void k_thread_b(void *arg) {
	while(1) {
		console_put_str("var_b:0x");
		console_put_int(var_b);
		console_put_str(" ");
	}
}

void u_prog_a(void) {
	while(1) {
		++var_a;
	}
}

void u_prog_b(void) {
	while(1) {
		++var_b;
	}
}
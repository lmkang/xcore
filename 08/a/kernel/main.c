#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"
#include "process.h"
#include "unistd.h"

void k_thread_a(void*);

void k_thread_b(void*);

void u_prog_a(void);

void u_prog_b(void);

int a_pid = 0, b_pid = 0;

int main() {
	put_str("hello,kernel!\n");
	init_all();
	
	enable_intr();// 打开中断,使时钟中断起作用
	
	process_execute(u_prog_a, "user_prog_a");
	process_execute(u_prog_b, "user_prog_b");
	
	console_put_str(" main_pid:0x");
	console_put_int(getpid());
	console_put_char('\n');
	thread_start("k_thread_a", 31, k_thread_a, "argA ");
	thread_start("k_thread_b", 8, k_thread_b, "argB ");
	
	while(1);
	return 0;
}

void k_thread_a(void *arg) {
	console_put_str(" thread_a_pid:0x");
	console_put_int(getpid());
	console_put_char('\n');
	console_put_str(" prog_a_pid:0x");
	console_put_int(a_pid);
	console_put_char('\n');
	while(1);
}

void k_thread_b(void *arg) {
	console_put_str(" thread_b_pid:0x");
	console_put_int(getpid());
	console_put_char('\n');
	console_put_str(" prog_b_pid:0x");
	console_put_int(b_pid);
	console_put_char('\n');
	while(1);
}

void u_prog_a(void) {
	a_pid = getpid();
	while(1);
}

void u_prog_b(void) {
	b_pid = getpid();
	while(1);
}
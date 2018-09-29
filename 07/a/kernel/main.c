#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"

void k_thread_a(void*);

void k_thread_b(void*);

int main() {
	put_str("hello,kernel!\n");
	init_all();
	thread_start("k_thread_a", 31, k_thread_a, "argA ");
	thread_start("k_thread_b", 8, k_thread_b, "argB ");
	enable_intr();// 打开中断,使时钟中断起作用
	while(1) {
		// console_put_str("Main ");
	}
	return 0;
}

void k_thread_a(void *arg) {
	char *param = (char*) arg;
	while(1) {
		enum intr_status old_status = get_intr_status();
		disable_intr();
		if(!ioq_empty(&keyboard_buf)) {
			console_put_str(param);
			char ch = ioq_getchar(&keyboard_buf);
			console_put_char(ch);
		}
		set_intr_status(old_status);
	}
}

void k_thread_b(void *arg) {
	char *param = (char*) arg;
	while(1) {
		enum intr_status old_status = get_intr_status();
		disable_intr();
		if(!ioq_empty(&keyboard_buf)) {
			console_put_str(param);
			char ch = ioq_getchar(&keyboard_buf);
			console_put_char(ch);
		}
		set_intr_status(old_status);
	}
}
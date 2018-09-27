#include "print.h"
#include "init.h"
#include "thread.h"

void k_thread_test(void*);

int main() {
	put_str("hello,kernel!\n");
	init_all();
	thread_start("k_thread_test", 31, k_thread_test, "argA");
	while(1) {
		
	}
	return 0;
}

void k_thread_test(void *arg) {
	char *param = (char*) arg;
	while(1) {
		put_str(param);
		put_str(" ");
	}
}
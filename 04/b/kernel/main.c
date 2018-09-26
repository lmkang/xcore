#include "print.h"
#include "init.h"

int main() {
	put_str("hello,kernel!\n");
	init_all();
	__asm__ __volatile__("sti"); // 为演示中断处理,在此临时开中断
	while(1) {
		
	}
	return 0;
}

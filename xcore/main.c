#include "print.h"
#include "init.h"

int main(struct multiboot *mboot_prt) {
	init_all();
	
	enable_intr();
	__asm__ __volatile__("int $20");
	
	put_str("hello, kernel!\n");
	
	return 0;
}
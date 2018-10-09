#include "print.h"
#include "init.h"

int main(struct multiboot *mboot_prt) {
	init_all();
	put_str("hello,kernel!a\b");
	return 0;
}
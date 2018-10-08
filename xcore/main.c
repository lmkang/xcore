#include "print.h"
#include "init.h"

int main(struct multiboot *mboot_prt) {
	init_all();
	for(int i =0; i < 500; i++) {
		put_str("test-");
		put_hex(i);
		put_char(' ');
	}
	return 0;
}
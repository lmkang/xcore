#include "print.h"

int main(struct multiboot *mboot_prt) {
	put_str("hello,kernel! \n");
	put_hex(0);
	put_char(' ');
	put_hex(0x12345678);
	put_char(' ');
	put_hex(0xb8000);
	put_char(' ');
	put_hex(0xa4bc5d8f);
	return 0;
}
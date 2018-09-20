#include "print.h"
int main() {
	put_str("hello,kernel!\n");
	put_int(0x0);
	put_char('\n');
	put_int(0xb8000);
	put_char('\n');
	put_int(0x12345678);
	while(1) {
		
	}
}
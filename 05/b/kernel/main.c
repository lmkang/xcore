#include "print.h"
#include "init.h"
#include "string.h"

int main() {
	put_str("hello,kernel!\n");
	init_all();
	char dst[20] = "hello,";
	char *src = "world!\n";
	strcat(dst, src);
	put_str(dst);
	while(1) {
		
	}
	return 0;
}
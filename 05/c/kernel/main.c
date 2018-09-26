#include "print.h"
#include "init.h"
#include "memory.h"

int main() {
	put_str("hello,kernel!\n");
	init_all();
	void *addr = alloc_kernel_page(3);
	put_str("alloc_kernel_page start vaddr is ");
	put_int((uint32_t) addr);
	put_str("\n");
	while(1) {
		
	}
	return 0;
}
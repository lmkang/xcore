#include "print.h"
#include "init.h"
#include "interrupt.h"
#include "multiboot.h"

#define CHECK_FLAG(flag, bit) ((flag) & (1 << (bit)))

void show_mmap(struct multiboot *mboot_ptr) {
	uint32_t *mem_size_addr = (uint32_t *) 0x90000;
	if (CHECK_FLAG(mboot_ptr->flags, 0)) {
        *mem_size_addr = (mboot_ptr->mem_lower * 1024) + (mboot_ptr->mem_upper * 1024);
        put_str("Total Memory: ");
        put_hex((*mem_size_addr) / (1024 * 1024) + 1);
		put_str("MB\n");
    }
	if(CHECK_FLAG(mboot_ptr->flags, 6)) {
		uint32_t mmap_addr = mboot_ptr->mmap_addr;
		uint32_t mmap_length = mboot_ptr->mmap_length;
		struct mmap_entry *mmap = (struct mmap_entry*) mmap_addr;
		for(; (uint32_t) mmap < mmap_addr + mmap_length; mmap++) {
			// base_addr_low + length_low
			put_str("mmap->base_addr_low: ");
			put_hex((uint32_t) mmap->base_addr_low);
			put_char('\n');
			put_str("mmap->length_low: ");
			put_hex((uint32_t) mmap->length_low);
			put_char('\n');
		}
	}
}

int main(struct multiboot *mboot_ptr) {
	init_all();
	
	//enable_intr();
	//__asm__ __volatile__("int $20");
	
	put_str("hello, kernel!\n");
	
	show_mmap(mboot_ptr);
	
	return 0;
}
#include "print.h"
#include "init.h"
#include "interrupt.h"
#include "multiboot.h"

#define CHECK_FLAG(flag, bit) ((flag) & (1 << (bit)))
#define TOTAL_MEM_SIZE_ADDR 0x90000

void get_total_mem(struct multiboot *mboot_ptr);

int main(struct multiboot *mboot_ptr) {
	// 初始化所有模块
	init_all();
	
	// 获取总物理内存容量,存在TOTAL_MEM_SIZE_ADDR地址处
	get_total_mem(mboot_ptr);
	
	// 打印总物理内存容量
	put_hex(*((uint32_t *) TOTAL_MEM_SIZE_ADDR));
	put_str("MB\n");
	
	//enable_intr();
	//__asm__ __volatile__("int $20");
	
	return 0;
}

// 获取总物理内存容量,存在TOTAL_MEM_SIZE_ADDR地址处
void get_total_mem(struct multiboot *mboot_ptr) {
	uint32_t *mem_size_addr = (uint32_t *) TOTAL_MEM_SIZE_ADDR;
	if (CHECK_FLAG(mboot_ptr->flags, 0)) {
        *mem_size_addr = ((mboot_ptr->mem_lower * 1024) + (mboot_ptr->mem_upper * 1024)) / (1024 * 1024) + 1;
    }
}
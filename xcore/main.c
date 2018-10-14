#include "types.h"
#include "global.h"
#include "init.h"
#include "print.h"
#include "multiboot.h"

#define CHECK_FLAG(flag, bit) ((flag) & (1 << (bit)))

extern struct multiboot *glb_mboot_ptr;

void kmain(struct multiboot *mboot_ptr);
void get_total_mem(struct multiboot *mboot_ptr);

// 内核栈
uint8_t kern_stack[STACK_SIZE];

// 建立临时页目录项,页表项,用来指向两个页
__attribute__((section(".init.data"))) uint32_t *pde_tmp = (uint32_t*) 0x1000;
__attribute__((section(".init.data"))) uint32_t *pte_low = (uint32_t*) 0x2000;
__attribute__((section(".init.data"))) uint32_t *pte_high = (uint32_t*) 0x3000;

__attribute__((section(".init.text"))) void entry() {
	pde_tmp[0] = (uint32_t) pte_low | PAGE_RW_W | PAGE_P;
	pde_tmp[GET_PDE_NUM(KERNEL_OFFSET)] = (uint32_t) pte_high | PAGE_RW_W | PAGE_P;
	// 4KB / 4B = 1024
	// 映射虚拟地址0xc0000000-0xc0400000到0x00000000-0x00400000的物理地址
	// 映射0x00000000-0x00400000的物理地址到虚拟地址0xc0000000-0xc0400000
	for(int i = 0; i < 1024; i++) {
		pte_low[i] = (i << 12) | PAGE_RW_W | PAGE_P;
		pte_high[i] = (i << 12) | PAGE_RW_W | PAGE_P;
	}
	// 开启分页
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(pde_tmp));
	uint32_t cr0 = 0;
	__asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
	cr0 |= 0x80000000;
	__asm__ __volatile__("mov %0, %%cr0" : : "r"(cr0));
	// 切换内核栈
	uint32_t kern_stack_top = ((uint32_t) kern_stack + STACK_SIZE) & 0xfffffff0;
	__asm__ __volatile__("mov %0, %%esp" : : "r"(kern_stack_top));
	glb_mboot_ptr += KERNEL_OFFSET;
	kmain(glb_mboot_ptr);
}

void kmain(struct multiboot *mboot_ptr) {
	// 获取总物理内存容量
	get_total_mem(mboot_ptr);
	
	// 初始化所有模块
	init_all();
	
	put_str("hello,kernel!\n");
	
	// 打印总物理内存容量
	put_str("Total Memory : ");
	put_hex(*((uint32_t*) (TOTAL_MEM_SIZE_ADDR + KERNEL_OFFSET)));
	put_str("MB\n");
	
	//__asm__ __volatile__("int $10");
	
	//put_hex(*(uint32_t*) 0xc0401200);
	
}

// 获取总物理内存容量,存在TOTAL_MEM_SIZE_ADDR地址处
void get_total_mem(struct multiboot *mboot_ptr) {
	uint32_t *mem_size_addr = (uint32_t *) (TOTAL_MEM_SIZE_ADDR + KERNEL_OFFSET);
	if (CHECK_FLAG(mboot_ptr->flags, 0)) {
        *mem_size_addr = ((mboot_ptr->mem_lower * 1024) + (mboot_ptr->mem_upper * 1024)) / (1024 * 1024) + 1;
    }
}
















































































#include "types.h"
#include "global.h"
#include "init.h"
#include "print.h"
#include "multiboot.h"
#include "memory.h"

#define CHECK_FLAG(flag, bit) ((flag) & (1 << (bit)))

extern struct multiboot *glb_mboot_ptr;

void kmain(struct multiboot *mboot_ptr);
void get_total_mem(struct multiboot *mboot_ptr);

// 内核栈
uint8_t kern_stack[STACK_SIZE];

// 建立临时页目录项,页表项,用来指向两个页
__attribute__((section(".init.data"))) uint32_t *pgd_tmp = (uint32_t*) 0x1000;
__attribute__((section(".init.data"))) uint32_t *pte_low1 = (uint32_t*) 0x2000;
__attribute__((section(".init.data"))) uint32_t *pte_low2 = (uint32_t*) 0x3000;
__attribute__((section(".init.data"))) uint32_t *pte_high1 = (uint32_t*) 0x4000;
__attribute__((section(".init.data"))) uint32_t *pte_high2 = (uint32_t*) 0x5000;

__attribute__((section(".init.text"))) void entry() {
	pgd_tmp[0] = (uint32_t) pte_low1 | PAGE_RW_W | PAGE_P_1;
	pgd_tmp[GET_PGD_INDEX(KERNEL_OFFSET)] = (uint32_t) pte_high1 | PAGE_RW_W | PAGE_P_1;
	pgd_tmp[1] = (uint32_t) pte_low2 | PAGE_RW_W | PAGE_P_1;
	pgd_tmp[GET_PGD_INDEX(KERNEL_OFFSET) + 1] = (uint32_t) pte_high2 | PAGE_RW_W | PAGE_P_1;
	// 映射虚拟地址0xc0000000-0xc0400000到0x00000000-0x00400000的物理地址
	// 映射0x00000000-0x00400000的物理地址到虚拟地址0xc0000000-0xc0400000
	for(uint32_t i = 0; i < PAGE_PTE_SIZE; i++) {
		pte_low1[i] = (i << 12) | PAGE_RW_W | PAGE_P_1;
		pte_high1[i] = (i << 12) | PAGE_RW_W | PAGE_P_1;
	}
	// 映射虚拟地址0xc0400000-0xc0800000到0x00400000-0x00800000的物理地址
	// 映射0x00400000-0x00800000的物理地址到虚拟地址0xc0400000-0xc0800000
	for(uint32_t i = 1024; i < PAGE_PTE_SIZE + 1024; i++) {
		pte_low2[i - 1024] = (i << 12) | PAGE_RW_W | PAGE_P_1;
		pte_high2[i - 1024] = (i << 12) | PAGE_RW_W | PAGE_P_1;
	}
	// 开启分页
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(pgd_tmp));
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
	
	// 初始化内存管理
	init_mem_pool(*((uint32_t*) P2V(TOTAL_MEM_SIZE_ADDR)));
	
	// 打印总物理内存容量
	printk("Total Memory : %xMB\n", *((uint32_t*) P2V(TOTAL_MEM_SIZE_ADDR)) / (1024 * 1024));
	
	//__asm__ __volatile__("int $10");
	
	uint32_t *test1 = (uint32_t*) kmalloc(1);
	if(test1 != NULL) {
		*test1 = 23;
		printk("test1 : %x\n", test1);
		printk("*test1 : %d\n", *test1);
	}
	
	uint32_t *test2 = (uint32_t*) kmalloc(3);
	if(test2 != NULL) {
		*test2 = 50;
		printk("test2 : %x\n", test2);
		printk("*test2 : %d\n", *test2);
	}
	
	kfree(test2, 3);
	
	uint32_t *test3 = (uint32_t*) kmalloc(16);
	if(test3 != NULL) {
		*test3 = 66;
		printk("test3 : %x\n", test3);
		printk("*test3 : %d\n", *test3);
	}
	
	//printk("user prog : %x\n", *((uint32_t*) 0x08048000));
	
	while(1); // 使CPU悬停在此
	
}

// 获取总物理内存容量,存在TOTAL_MEM_SIZE_ADDR地址处
void get_total_mem(struct multiboot *mboot_ptr) {
	uint32_t *mem_size_addr = (uint32_t *) P2V(TOTAL_MEM_SIZE_ADDR);
	if (CHECK_FLAG(mboot_ptr->flags, 0)) {
        *mem_size_addr = mboot_ptr->mem_lower * 1024 + mboot_ptr->mem_upper * 1024 + 1024 * 1024;
    }
}
















































































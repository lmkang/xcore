#include "types.h"
#include "global.h"
#include "print.h"
#include "bitmap.h"

// 内核页目录数组
pgd_t pgd_kern[PAGE_PGD_SIZE] __attribute__((aligned(PAGE_SIZE)));

// 内核页表数组
pte_t pte_kern[PAGE_PTE_COUNT][PAGE_PTE_SIZE] __attribute__((aligned(PAGE_SIZE)));

// 内存池
struct memory_pool {
	struct bitmap *btmp;
	uint32_t start_addr;
};

// 内核物理内存池
struct memory_pool kernel_pool;
// 用户物理内存池
struct memory_pool user_pool;

// 切换页目录
void switch_pgd(uint32_t pgd) {
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(pgd));
}

// 初始化内核虚拟地址
void init_kernel_vmm() {
	uint32_t kern_pte_base = GET_PGD_INDEX(KERNEL_OFFSET);
	for(uint32_t i = kern_pte_base, j = 0; i < PAGE_PTE_COUNT + kern_pte_base; i++, j++) {
		pgd_kern[i] = V2P((uint32_t) pte_kern[j]) | PAGE_P_1 | PAGE_RW_W;
	}
	uint32_t *pte = (uint32_t*) pte_kern;
	for(uint32_t i = 0; i < PAGE_PTE_COUNT * PAGE_PTE_SIZE; i++) {
		pte[i] = (i << 12) | PAGE_P_1 | PAGE_RW_W;
	}
	switch_pgd(V2P((uint32_t) pgd_kern));
	
	printk("init_kernel_vmm done\n");
}












































































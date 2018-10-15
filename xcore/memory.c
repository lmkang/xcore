#include "types.h"
#include "global.h"

// 内核页目录数组
pgd_t pgd_kern[PAGE_PGD_SIZE] __attribute__(aligned(PAGE_SIZE));

// 内核页表数组
pte_t pte_kern[PAGE_PGD_SIZE][PAGE_PTE_SIZE] __attribute__(aligned(PAGE_SIZE));

// 切换页目录
void switch_pgd(uint32_t pgd) {
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(pgd));
}

// 初始化内核虚拟地址
void init_kernel_vmm() {
	uint32_t kern_pte_base = GET_PGD_INDEX(KERNEL_OFFSET);
	for(uint32_t i = kern_pte_base, j = 0; i < PAGE_PTE_SIZE + kern_pte_base; i++, j++) {
		pgd_kern[i] = V2P(pte_kern[j]) | PAGE_P_1 | PAGE_RW_W;
	}
	uint32_t *pte = (uint32_t*) pte_kern;
	// 不映射第0页,便于跟踪NULL指针
	for(uint32_t i = 0; PAGE_PTE_SIZE * ) {
		
	}
}













































































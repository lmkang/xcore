#include "types.h"
#include "global.h"
#include "print.h"
#include "bitmap.h"
#include "debug.h"

// 内核页目录数组
pgd_t pgd_kern[PAGE_PGD_SIZE] __attribute__((aligned(PAGE_SIZE)));

// 内核页表数组
pte_t pte_kern[PAGE_PTE_COUNT][PAGE_PTE_SIZE] __attribute__((aligned(PAGE_SIZE)));

// 物理内存池
struct memory_pool {
	struct bitmap pool_btmp;
	uint32_t paddr_start;
	uint32_t pool_size; // 字节大小
};

// 虚拟地址管理
struct virtual_addr {
	struct bitmap vaddr_btmp; // 虚拟地址用到的位图结构
	uint32_t vaddr_start; // 虚拟地址起始地址
};

// 内核物理内存池
struct memory_pool kernel_pool;
// 用户物理内存池
struct memory_pool user_pool;

struct virtual_addr kernel_vaddr;

// 切换页目录
void switch_pgd(uint32_t pgd) {
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(pgd));
}

// 初始化内核虚拟地址
void init_kernel_vmm() {
	uint32_t kern_pte_base = GET_PGD_INDEX(KERNEL_OFFSET);
	uint32_t kern_pte_count = PAGE_PTE_COUNT / 2;
	// 内核页目录表最多 1023 - 768 + 1 = 256
	if(kern_pte_count > 256) {
		kern_pte_count = 256;
	}
	for(uint32_t i = kern_pte_base, j = 0; i < kern_pte_count + kern_pte_base; i++, j++) {
		pgd_kern[i] = V2P((uint32_t) pte_kern[j]) | PAGE_P_1 | PAGE_RW_W;
	}
	/*
	uint32_t *pte = (uint32_t*) pte_kern;
	for(uint32_t i = 0; i < kern_pte_count * PAGE_PTE_SIZE; i++) {
		pte[i] = (i << 12) | PAGE_P_1 | PAGE_RW_W;
	}
	*/
	uint32_t *pte = (uint32_t*) pte_kern;
	for(uint32_t i = 0; i < 2 * PAGE_PTE_SIZE; i++) {
		pte[i] = (i << 12) | PAGE_P_1 | PAGE_RW_W;
	}
	switch_pgd(V2P((uint32_t) pgd_kern));
	
	printk("init_kernel_vmm done\n");
}

// 初始化物理内存管理
void init_mem_pool(uint32_t mem_size) {
	uint32_t total_mem_size = mem_size - KERNEL_END_PADDR;
	uint32_t total_pages = total_mem_size / PAGE_SIZE;
	
	uint32_t kernel_pages = total_pages / 2;
	uint32_t kernel_btmp_len = kernel_pages / 8;
	
	kernel_pool.paddr_start = KERNEL_END_PADDR;
	kernel_pool.pool_size = kernel_pages * PAGE_SIZE;
	kernel_pool.pool_btmp.byte_len = kernel_btmp_len;
	kernel_pool.pool_btmp.bits = (void*) P2V(MM_BITMAP_PADDR);
	
	printk("paddr_bits : %x\n", kernel_pool.pool_btmp.bits);
	
	init_bitmap(&kernel_pool.pool_btmp);
	
	kernel_vaddr.vaddr_start = KERNEL_VADDR_START;
	kernel_vaddr.vaddr_btmp.byte_len = kernel_btmp_len;
	kernel_vaddr.vaddr_btmp.bits = (void*) P2V(MM_BITMAP_PADDR + kernel_btmp_len);
	
	printk("vaddr_bits : %x\n", kernel_vaddr.vaddr_btmp.bits);
	
	init_bitmap(&kernel_vaddr.vaddr_btmp);
}

// 获取虚拟地址
static void *get_vaddr(uint32_t page_count) {
	int v_index = alloc_bitmap(&kernel_vaddr.vaddr_btmp, page_count);
	if(v_index == -1) {
		return NULL;
	}
	return (void*) (kernel_vaddr.vaddr_start + v_index * PAGE_SIZE);
}

// 获取物理地址
static void *get_paddr(uint32_t page_count) {
	int p_index = alloc_bitmap(&kernel_pool.pool_btmp, page_count);
	if(p_index == -1) {
		return NULL;
	}
	return (void*) (kernel_pool.paddr_start + p_index * PAGE_SIZE);
}

// 将虚拟地址映射到物理地址
static void vp_map(void *vaddr, void *paddr) {
	ASSERT(vaddr != NULL && paddr != NULL);
	uint32_t _vaddr = (uint32_t) vaddr;
	uint32_t _paddr = (uint32_t) paddr;
	uint32_t pgd_index = GET_PGD_INDEX(_vaddr);
	uint32_t *pte = (uint32_t*) (P2V(pgd_kern[pgd_index]) & 0xfffffff0);
	uint32_t pte_index = GET_PTE_INDEX(_vaddr);
	for(int i = 0; i < 1024; i++) {
		pte[i] = _paddr | PAGE_P_1 | PAGE_RW_W;
	}
	//pte[pte_index] = _paddr | PAGE_P_1 | PAGE_RW_W;
	
	printk("_vaddr : %x\n", _vaddr);
	printk("_paddr : %x\n", _paddr);
}

// 分配size个页(4KB)的空间
void *kmalloc(uint32_t size) {
	ASSERT(size > 0);
	void *vaddr = get_vaddr(size);
	if(vaddr == NULL) {
		return NULL;
	}
	void *paddr = get_paddr(size);
	if(paddr == NULL) {
		return NULL;
	}
	while(size-- > 0) {
		vp_map(vaddr, paddr);
		vaddr += PAGE_SIZE;
		paddr += PAGE_SIZE;
	}
	return vaddr;
}

// 释放以虚拟地址vaddr为起始的size个页框
void kfree(void *vaddr, uint32_t size) {
	ASSERT(vaddr != NULL && size > 0);
	
}






































































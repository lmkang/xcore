#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
#include "list.h"

// 物理内存池标记,用于判断用哪个内存池
enum pool_flag {
	PF_KERNEL = 1, // 内核物理内存池
	PF_USER = 2 // 用户物理内存池
};

#define PG_P_1 1 // 页表项或页目录项存在属性位,存在
#define PG_P_0 0 // 页表项或页目录项存在属性位,不存在
#define PG_RW_R 0 // R/W属性位值,读/执行
#define PG_RW_W 2 // R/W属性位值,读/写/执行
#define PG_US_S 0 // U/S属性位值,系统级
#define PG_US_U 4 // U/S属性位值,用户级

#define BLOCK_DESC_COUNT 7 // 内存块描述符个数

// 虚拟地址池,用于虚拟地址管理
struct virtual_addr {
	struct bitmap vaddr_btmp; // 虚拟地址用到的位图结构
	uint32_t vaddr_start; // 虚拟地址起始地址
};

// 内存块
struct mem_block {
	struct list_ele free_ele;
};

// 内存块描述符
struct mem_block_desc {
	uint32_t block_size; // 内存块大小
	uint32_t block_count; // arena可容纳的mem_block的数目
	struct list free_list;
};

extern struct memory_pool kernel_pool, user_pool;

void init_memory(void);

void * malloc_page(enum pool_flag pf, uint32_t page_count);

void * alloc_kernel_page(uint32_t page_count);

uint32_t * get_pte_ptr(uint32_t vaddr);

uint32_t * get_pde_ptr(uint32_t vaddr);

void * alloc_user_page(uint32_t page_count);

void * get_a_page(enum pool_flag pf, uint32_t vaddr);

uint32_t addr_v2p(uint32_t vaddr);

void init_block_desc(struct mem_block_desc *desc_array);

void mfree_page(enum pool_flag pf, void *vaddr, uint32_t page_count);

void * sys_malloc(uint32_t size);

void sys_free(void *ptr);

#endif
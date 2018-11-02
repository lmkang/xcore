#ifndef __MEMORY_H
#define __MEMORY_H

#include "bitmap.h"
#include "global.h"
#include "list.h"

#define MEM_BLOCK_DESC_COUNT 7 // 内存块描述符个数

// 内存池类型
enum pool_flag {
	PF_KERNEL = 1,
	PF_USER = 2
};

// 虚拟地址池
struct vaddr_pool {
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
	uint32_t block_count; // arena可容纳的mem_block的数量
	struct list free_list; // 可用的mem_block链表
};

void mm_init();

void init_block_desc(struct mem_block_desc *desc_arr);

uint32_t kern_v2p(uint32_t vaddr);

void *kmalloc(uint32_t size, enum pool_flag pf);

void kfree(void *vaddr, uint32_t size);

void *get_kernel_pages(uint32_t size);

void *get_prog_pages(uint32_t vaddr, uint32_t size);

void *sys_malloc(uint32_t size);

void sys_free(void *ptr);

#endif
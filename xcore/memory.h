#ifndef __MEMORY_H
#define __MEMORY_H

#include "bitmap.h"
#include "global.h"

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

void init_kernel_vmm();

void init_mem_pool(uint32_t mem_size);

uint32_t kern_v2p(uint32_t vaddr);

void *kmalloc(uint32_t size, enum pool_flag pf);

void kfree(void *vaddr, uint32_t size);

void *get_user_pages(uint32_t size);

void *get_kernel_pages(uint32_t size);

void *get_pages(uint32_t vaddr, uint32_t size);

#endif
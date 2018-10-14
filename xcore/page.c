#include "types.h"
#include "string.h"
#include "global.h"

// 创建页目录表和页表
static void setup_page() {
	// 1 将页目录表(PDT)清0
	memset((void*) PDT_BASE_ADDR, 0, 4096);
	// 2 创建页目录项(PDE)
	// 将0xc03fffff以下的地址和0x003fffff以下的地址都指向相同的页表
	uint32_t *pde = (uint32_t*) PDT_BASE_ADDR;
	*pde = (PDT_BASE_ADDR + 0x1000) | (PAGE_P | PAGE_RW_W | PAGE_US_U);
	*(pde + 0xc00) = (PDT_BASE_ADDR + 0x1000) | (PAGE_P | PAGE_RW_W | PAGE_US_U);
	*(pde + 4092) = PDT_BASE_ADDR | (PAGE_P | PAGE_RW_W | PAGE_US_U);
	// 3 创建页表项(PTE)
	uint32_t *pte = (uint32_t*) (PDT_BASE_ADDR + 0x1000);
	for(int i = 0; i < 1024; i++) {
		*(pte + i * 4) = 4096 * i + (PAGE_P | PAGE_RW_W | PAGE_US_U);
	}
	// 4 创建内核其他页表的PDE
	// 范围是769-1022的所有页目录项
	pde = (uint32_t*) PDT_BASE_ADDR;
	int index = 0;
	for(int i = 769; i < 1023; i++) {
		*(pde + i * 4) = (PDT_BASE_ADDR + 0x2000 + (PAGE_P | PAGE_RW_W | PAGE_US_U)) + (index++) * 0x1000;
	}
}

// 初始化分页
void init_page() {
	setup_page();
}
#include "types.h"
#include "string.h"

// page
#define PDT_BASE_ADDR 0x400000 // page diretory table base address

// 初始化分页
void init_page() {
	// 1 将页目录表清0
	memset((void*) PDT_BASE_ADDR, 0, 4096);
	// 2 创建页目录项
	// 将0xc03fffff以下的地址和0x003fffff以下的地址都指向相同的页表
	uint32_t *pde = (uint32_t*) PDT_BASE_ADDR;
	*pde = (PDT_BASE_ADDR + 0x1000) | 0x7;
	*(pde + 0xc00) = (PDT_BASE_ADDR + 0x1000) | 0x7;
	*(pde + 4092) = PDT_BASE_ADDR | 0x7;
}
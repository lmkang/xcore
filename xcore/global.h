#ifndef __GLOBAL_H
#define __GLOBAL_H

// selector
#define SELECTOR_KERNEL_CODE (0x1 << 3)
#define SELECTOR_KERNEL_DATA (0x2 << 3)
#define SELECTOR_VIDEO_DATA (0x3 << 3)
#define SELECTOR_USER_CODE (0x4 << 3 + 3)
#define SELECTOR_USER_DATA (0x5 << 3 + 3)

// page
#define PAGE_SIZE 4096 // 页大小(4KB)
#define PAGE_P_1 1 // present
#define PAGE_RW_W 10 // read/write
#define PAGE_US_U 100 // user
#define PAGE_PGD_SIZE 1024 // 页目录大小(单位:4B)
#define PAGE_PTE_SIZE 1024 // 页表大小(单位:4B)
#define PAGE_PTE_COUNT 128 // 内核页表数目

// PDE,PTE,OFFSET
#define GET_PGD_INDEX(x) (((x) >> 22) & 0x3ff)
#define GET_PTE_INDEX(x)  (((x) >> 12) & 0x3ff)
#define GET_OFFSET_INDEX(x)  ((x) & 0xfff)

// kernel offset
#define KERNEL_OFFSET 0xc0000000

// stack size
#define STACK_SIZE 8192

// total memory size is placed at 0x90000
#define TOTAL_MEM_SIZE_ADDR 0x90000

// virtual address to physic address
#define V2P(x) ((x) - KERNEL_OFFSET)
// physic address to virtual address
#define P2V(x) ((x) + KERNEL_OFFSET)

#endif
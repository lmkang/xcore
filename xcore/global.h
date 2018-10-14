#ifndef __GLOBAL_H
#define __GLOBAL_H

// selector
#define SELECTOR_KERNEL_CODE (0x1 << 3)
#define SELECTOR_KERNEL_DATA (0x2 << 3)
#define SELECTOR_VIDEO_DATA (0x3 << 3)
#define SELECTOR_USER_CODE (0x4 << 3 + 3)
#define SELECTOR_USER_DATA (0x5 << 3 + 3)

// page
#define PAGE_SIZE 4096
#define PAGE_P 1
#define PAGE_RW_W 10
#define PAGE_US_U 100
#define PDT_BASE_ADDR 0x400000 // page diretory table base address

// PDE,PTE,OFFSET
#define GET_PDE_NUM(x) (((x) >> 22) & 0x3ff)
#define GET_PTE_NUM(x)  (((x) >> 12) & 0x3ff)
#define GET_OFFSET_NUM(x)  ((x) & 0xfff)

#define KERNEL_OFFSET 0xc0000000

#define STACK_SIZE 8192

#define TOTAL_MEM_SIZE_ADDR 0x90000

#endif
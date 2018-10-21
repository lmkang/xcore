#ifndef __GLOBAL_H
#define __GLOBAL_H

// selector
#define SELECTOR_KERNEL_CODE (0x1 << 3)
#define SELECTOR_KERNEL_DATA (0x2 << 3)
#define SELECTOR_VIDEO_DATA (0x3 << 3)
#define SELECTOR_TSS (0x4 << 3)
#define SELECTOR_USER_CODE (0x5 << 3 + 3)
#define SELECTOR_USER_DATA (0x6 << 3 + 3)

// page
#define PAGE_SIZE 4096 // 页大小(4KB)
#define PAGE_P_1 0x1 // present
#define PAGE_RW_W 0x2 // read/write
#define PAGE_US_U 0x4 // user
#define PAGE_PGD_SIZE 1024 // 页目录大小(单位:4B)
#define PAGE_PTE_SIZE 1024 // 页表大小(单位:4B)
#define PAGE_PTE_COUNT 1024 // 内核页表数目(最大是1024,支持4GB物理内存)

// eflags
#define EFLAGS_MBS (1 << 1) // 此项必须设置
#define EFLAGS_IF_1 (1 << 9) // IF为1,开中断
#define EFLAGS_IF_0 0 // IF为0,关中断
#define EFLAGS_IOPL_3 (3 << 12) // 用于测试用户程序在非系统调用下进行IO
#define EFLAGS_IOPL_0 0 // IOPL 0

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

// memory management bitmap physic address
#define MM_BITMAP_PADDR 0x7c0000

// kernel end physic address(8MB)
#define KERNEL_END_PADDR 0x800000

// kernel virtual address start
#define KERNEL_VADDR_START 0xc0800000

// user virtual address start
#define USER_VADDR_START 0x08048000

// virtual address to physic address
#define V2P(x) ((x) - KERNEL_OFFSET)
// physic address to virtual address
#define P2V(x) ((x) + KERNEL_OFFSET)

#define OFFSET(struct_type, member_name) \
	((int) (&((struct_type*) 0)->member_name))
	
#define ELE2ENTRY(struct_type, member_name, ele_ptr) \
	((struct_type*) ((int) ele_ptr - OFFSET(struct_type, member_name)))

#endif
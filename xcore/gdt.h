#ifndef __GDT_H
#define __GDT_H

#include "types.h"

// 段描述符
struct gdt_entry {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_mid;
	uint8_t attr_low;
	uint8_t attr_high;
	uint8_t base_high;
}__attribute__((packed))__;

// 段描述符寄存器
struct gdt_ptr {
	uint16_t limit;
	uint32_t base;
}__attribute__((packed));

void init_gdt();

#endif
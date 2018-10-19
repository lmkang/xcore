#include "types.h"
#include "global.h"
#include "print.h"

#define GDT_ENTRY_COUNT 6

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

// gdt数组
struct gdt_entry gdt_entries[GDT_ENTRY_COUNT];
// gdt指针
struct gdt_ptr *gdt_ptr;

// 设置段描述符
static void set_gdt_entry(uint16_t index, uint32_t base, uint32_t limit, 
	uint8_t attr_low, uint8_t attr_high) {
		
	gdt_entries[index].base_low = (base  & 0xffff);
	gdt_entries[index].base_mid = ((base >> 16) & 0xff);
	gdt_entries[index].base_high = ((base >> 24) & 0xff);
	
	gdt_entries[index].limit_low = ((limit >> 12) & 0xffff);
	gdt_entries[index].attr_high = ((limit >> 28) & 0x0f);
	
	gdt_entries[index].attr_high |= (attr_high & 0xf0);
	
	gdt_entries[index].attr_low = attr_low;
}

void flush_gdt(uint32_t gdt_entry) {
	__asm__ __volatile__(" \
		mov %0, %%eax; \
		lgdt (%%eax); \
		mov $0x10, %%ax; \
		mov %%ax, %%ds; \
		mov %%ax, %%es; \
		mov %%ax, %%fs; \
		mov %%ax, %%ss; \
		mov $0x18, %%ax; \
		mov %%ax, %%gs; \
		ljmp $0x08,$flush; \
		flush: " \
		: : "g"(gdt_entry) : "memory" \
	);
}

// 初始化gdt
void init_gdt() {
	// 0x9a: conforming, 0x98: no-conforming(DPL=00)
	// 0xfa: conforming, 0xf8: no-conforming(DPL=11)
	// 0x92: read/write data segment(DPL=00)
	// 0xf2: read/write data segment(DPL=11)
	set_gdt_entry(0, 0, 0, 0, 0); // Null segment
	set_gdt_entry(1, 0, 0xffffffff, 0x98, 0xcf); // Code segment
	set_gdt_entry(2, 0, 0xffffffff, 0x92, 0xcf); // Data segment
	set_gdt_entry(3, 0xb8000 + KERNEL_OFFSET, 0x7fff, 0x92, 0xc0); // Video segment
	set_gdt_entry(4, 0, 0xffffffff, 0xf8, 0xcf); // User mode code segment
	set_gdt_entry(5, 0, 0xffffffff, 0xf2, 0xcf); // User mode data segment
	
	gdt_ptr->limit = (sizeof(struct gdt_entry) * GDT_ENTRY_COUNT) - 1;
	gdt_ptr->base = (uint32_t) gdt_entries;
	
	flush_gdt((uint32_t) gdt_ptr);
	
	put_str("init_gdt done\n");
}
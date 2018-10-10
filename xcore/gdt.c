#include "gdt.h"

#define GDT_ENTRY_COUNT 6

// gdt数组
struct gdt_entry gdt_entries[GDT_ENTRY_COUNT];
// gdt指针
struct gdt_ptr *gdt_ptr;

// 刷新gdt
extern void flush_gdt(uint32_t);

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

// 初始化gdt
void init_gdt() {
	gdt_ptr->limit = (sizeof(gdt_entries) * GDT_ENTRY_COUNT) - 1;
	gdt_ptr->base = (uint32_t) &gdt_entries;
	
	// 0x9a: conforming, 0x98: no-conforming(DPL=00)
	// 0xfa: conforming, 0xf8: no-conforming(DPL=11)
	// 0x92: read/write data segment(DPL=00)
	// 0xf2: read/write data segment(DPL=11)
	set_gdt_entry(0, 0, 0, 0, 0); // Null segment
	set_gdt_entry(1, 0, 0xffffffff, 0x98, 0xcf); // Code segment
	set_gdt_entry(2, 0, 0xffffffff, 0x92, 0xcf); // Data segment
	set_gdt_entry(3, 0xb8000, 0x7fff, 0x92, 0xc0); // Video segment
	set_gdt_entry(4, 0, 0xffffffff, 0xf8, 0xcf); // User mode code segment
	set_gdt_entry(5, 0, 0xffffffff, 0xf2, 0xcf); // User mode data segment
	
	flush_gdt((uint32_t) gdt_ptr);
}
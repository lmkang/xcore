#include "types.h"
#include "global.h"
#include "print.h"
#include "thread.h"
#include "string.h"

#define GDT_ENTRY_COUNT 7

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

// 任务状态段TSS结构
struct tss {
	uint32_t backlink;
	uint32_t *esp0;
	uint32_t ss0;
	uint32_t *esp1;
	uint32_t ss1;
	uint32_t *esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t (*eip) (void);
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;
	uint32_t cs;
	uint32_t ss;
	uint32_t ds;
	uint32_t fs;
	uint32_t gs;
	uint32_t ldt;
	uint32_t trace;
	uint32_t io_base;
};

// gdt数组
static struct gdt_entry gdt_entries[GDT_ENTRY_COUNT];
// gdt指针
static struct gdt_ptr *gdt_ptr;

// TSS
static struct tss tss;

// 设置段描述符
static void set_gdt_entry(uint16_t index, uint32_t base, uint32_t limit, 
	uint8_t attr_low, uint8_t attr_high) {
		
	gdt_entries[index].base_low = (base  & 0xffff);
	gdt_entries[index].base_mid = ((base >> 16) & 0xff);
	gdt_entries[index].base_high = ((base >> 24) & 0xff);
	
	gdt_entries[index].limit_low = (limit & 0xffff);
	
	gdt_entries[index].attr_high = ((limit >> 16) & 0x0f);
	gdt_entries[index].attr_high |= (attr_high & 0xf0);
	
	gdt_entries[index].attr_low = attr_low;
}

static void flush_gdt(uint32_t gdt_entry) {
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

// 更新TSS中esp0字段的值为pthread的0级栈
void update_tss_esp(struct task_struct *pthread) {
	tss.esp0 = (uint32_t*) ((uint32_t) pthread + PAGE_SIZE);
}

// 初始化gdt
void init_gdt() {
	// 0x9a: conforming, 0x98: no-conforming(DPL=00)
	// 0xfa: conforming, 0xf8: no-conforming(DPL=11)
	// 0x92: read/write data segment(DPL=00)
	// 0xf2: read/write data segment(DPL=11)
	set_gdt_entry(0, 0, 0, 0, 0); // Null segment
	set_gdt_entry(1, 0, 0xfffff, 0x98, 0xc0); // Code segment
	set_gdt_entry(2, 0, 0xfffff, 0x92, 0xc0); // Data segment
	set_gdt_entry(3, 0xb8000 + KERNEL_OFFSET, 0x7fff, 0x92, 0x40); // Video segment
	set_gdt_entry(5, 0, 0xfffff, 0xf8, 0xc0); // User mode code segment
	set_gdt_entry(6, 0, 0xfffff, 0xf2, 0xc0); // User mode data segment
	
	// TSS segment
	uint32_t tss_size = sizeof(tss);
	memset(&tss, 0, tss_size);
	tss.ss0 = SELECTOR_KERNEL_DATA;
	tss.io_base = tss_size;
	set_gdt_entry(4, (uint32_t*) &tss, tss_size - 1, 0x89, 0x80);
	
	gdt_ptr->limit = (sizeof(struct gdt_entry) * GDT_ENTRY_COUNT) - 1;
	gdt_ptr->base = (uint32_t) gdt_entries;
	
	flush_gdt((uint32_t) gdt_ptr);
	
	__asm__ __volatile__("ltr %w0" : : "r"(SELECTOR_TSS));
	
	put_str("init_gdt done\n");
	put_str("init_tss done\n");
}
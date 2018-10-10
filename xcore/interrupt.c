#include "types.h"
#include "global.h"
#include "print.h"
#include "global.h"
#include "interrupt.h"
#include "x86.h"

// idt entry attribute
#define IDT_ENTRY_P 1
#define IDT_ENTRY_DPL0 0
#define IDT_ENTRY_DPL3 3
#define IDT_ENTRY_32_TYPE 0xe // 32位门
#define IDT_ENTRY_16_TYPE 0x6 // 16位门,目前用不到
#define IDT_ENTRY_ATTR_DPL0 ((IDT_ENTRY_P << 7) + (IDT_ENTRY_DPL0 << 5) + IDT_ENTRY_32_TYPE)
#define IDT_ENTRY_ATTR_DPL3 ((IDT_ENTRY_P << 7) + (IDT_ENTRY_DPL3 << 5) + IDT_ENTRY_32_TYPE)

#define PIC_MASTER_CTRL 0x20 // 主片的控制端口
#define PIC_MASTER_DATA 0x21 // 主片的数据端口
#define PIC_SLAVE_CTRL 0xa0 // 从片的控制端口
#define PIC_SLAVE_DATA 0xa1 // 从片的数据端口

#define IDT_ENTRY_COUNT 0x81 // 支持的中断数

#define EFLAGS_IF 0x00000200 // eflags寄存器中的if位为1
#define GET_EFLAGS(EFLAGS_VAR) __asm__ __volatile__("pushfl; popl %0" : "=g"(EFLAGS_VAR))

// 中断描述符表
struct idt_entry {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t reserved;
	uint8_t attribute;
	uint16_t offset_high;
}__attribute__((packed));

char *intr_names[IDT_ENTRY_COUNT]; // 中断名称数组
static struct idt_entry idt_entries[IDT_ENTRY_COUNT]; // 中断描述符表数组
extern void* intr_offsets[IDT_ENTRY_COUNT]; // 中断处理函数入口地址数组
void* intr_handlers[IDT_ENTRY_COUNT]; // 中断处理函数

// 设置中断描述符
static void set_idt_entry(struct idt_entry *idt_entry, uint8_t attribute, void *offset) {
	idt_entry->offset_low = (uint32_t) offset & 0x0000ffff;
	idt_entry->selector = SELECTOR_KERNEL_CODE; // 内核代码段选择子
	idt_entry->reserved = 0;
	idt_entry->attribute = attribute;
	idt_entry->offset_high = ((uint32_t) offset & 0xffff0000) >> 16;
}

// 初始化中断描述符表
static void init_idt_entries(void) {
	for(int i = 0; i < IDT_ENTRY_COUNT; i++) {
		if(i != 0x80) {
			set_idt_entry(&idt_entries[i], IDT_ENTRY_ATTR_DPL0, intr_offsets[i]);
		}
	}
	// 单独处理系统调用,系统调用对应的中断门DPL为3
	// 中断处理程序为单独的syscal_handler
	//set_idt_entry(&idt_entries[0x80], IDT_ENTRY_ATTR_DPL3, syscall_handler);
	put_str("init_idt_entries done\n");
}

// 初始化可编程中断控制器8259A
static void init_pic(void) {
	// 初始化主片
	outb(PIC_MASTER_CTRL, 0x11); // ICW1:边沿触发,级联8259,需要ICW4
	outb(PIC_MASTER_DATA, 0x20); // ICW2:起始中断向量号为0x20
	outb(PIC_MASTER_DATA, 0x04); // ICW3:IR2接从片
	outb(PIC_MASTER_DATA, 0x01); // ICW4:8086模式,正常EOI
	// 初始化从片
	outb(PIC_SLAVE_CTRL, 0x11); // ICW1:边沿触发,级联8259,需要ICW4
	outb(PIC_SLAVE_DATA, 0x28); // ICW2:起始中断向量号为0x28
	outb(PIC_SLAVE_DATA, 0x02); // ICW3:设置从片连接到主片的IR2引脚
	outb(PIC_SLAVE_DATA, 0x01); // ICW4:8086模式,正常EOI
	
	// IRQ0,IRQ1,IRQ2,其他全部关闭
	outb(PIC_MASTER_DATA, 0xf8);
	// IRQ14(硬盘控制器)
	outb(PIC_SLAVE_DATA, 0xbf);
	
	put_str("init_pic done\n");
}

// 通用的中断处理函数,一般用在异常出现时的处理
static void general_intr_handler(uint8_t vec_no) {
	// IRQ7和IRQ15会产生伪中断,无需处理
	// 0x2f是从片8259A上的最后一个IRQ引脚,保留项
	if(vec_no == 0x27 || vec_no == 0x2f) {
		return;
	}
	// 将光标位置设为0,从屏幕左上角清出一片打印异常信息的区域,方便阅读
	set_cursor(0);
	int cursor_pos = 0;
	while(cursor_pos < 320) {
		put_char(' ');
		++cursor_pos;
	}
	set_cursor(0);
	put_str("!!! exception message begin !!!\n");
	set_cursor(88); // 从第2行第8个字符开始打印
	put_str(intr_names[vec_no]);
	put_str("\n");
	if(vec_no == 0xe) { // 如果是pagefault,将缺失的地址打印
		int page_fault_vaddr = 0;
		__asm__ __volatile__("movl %%cr2, %0" : "=r"(page_fault_vaddr)); // cr2存放page_fault的地址
		put_str("page fault addr is ");
		put_hex(page_fault_vaddr);
		put_str("\n");
	}
	put_str("!!! exception message end !!!\n");
}

// 初始化一般中断处理函数注册和异常名称注册
static void init_idt_exception(void) {
	for(int i = 0; i < IDT_ENTRY_COUNT; i++) {
		intr_handlers[i] = general_intr_handler;
		intr_names[i] = "unknown";
	}
	intr_names[0] = "#DE Divide Error";
	intr_names[1] = "#DB Debug Exception";
	intr_names[2] = "#NMI None Maskable Interrupt";
	intr_names[3] = "#BP Breakpoint Exception";
	intr_names[4] = "#OF Overflow Exception";
	intr_names[5] = "#BR Bound Range Exceeded Exception";
	intr_names[6] = "#UD Invalid Opcode Exception";
	intr_names[7] = "#NM Device Not Available Exception";
	intr_names[8] = "#DF Double Fault Exception";
	intr_names[9] = "#CSO Coprocessor Segment Overrun";
	intr_names[10] = "#TS Invalid TSS Exception";
	intr_names[11] = "#NP Segment Not Present";
	intr_names[12] = "#SS Stack Fault Exception";
	intr_names[13] = "#GP General Protection Exception";
	intr_names[14] = "#PF Page Fault Exception";
	// intr_names[15]是Intel保留项,未使用
	intr_names[16] = "#MF x87 FPU Floating Point Error";
	intr_names[17] = "#AC Alignment Check Exception";
	intr_names[18] = "MC Machine Check Exception";
	intr_names[19] = "#XF SIMD Floating Point Exception";
}

// 初始化中断的所有初始化工作
void init_idt(void) {
	init_idt_entries(); // 初始化中断描述符表
	init_idt_exception(); // 初始化异常名并注册中断处理函数
	init_pic(); // 初始化8259A
	
	// 加载idt
	uint64_t idt_operand = ((sizeof(idt_entries) - 1) | ((uint64_t) (uint32_t) idt_entries << 16));
	__asm__ __volatile__("lidt %0" : : "m"(idt_operand));
	
	put_str("init_idt done\n");
}

// 在中断处理程序数组第vec_no个元素中注册中断处理程序func
void register_intr_handler(uint8_t vec_no, void *func) {
	intr_offsets[vec_no] = func;
}

// 获取当前中断状态
enum intr_status get_intr_status(void) {
	uint32_t eflags = 0;
	GET_EFLAGS(eflags);
	return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

// 开中断
void enable_intr(void) {
	if(INTR_OFF == get_intr_status()) {
		__asm__ __volatile__("sti");
	}
}

// 关中断
void disable_intr(void) {
	if(INTR_ON == get_intr_status()) {
		__asm__ __volatile__("cli" : : : "memory");
	}
}

// 将中断状态设置为status
void set_intr_status(enum intr_status status) {
	if(status & INTR_ON) {
		enable_intr();
	} else {
		disable_intr();
	}
}
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

// PIC
#define PIC_MASTER_CTRL 0x20 // 主片的控制端口
#define PIC_MASTER_DATA 0x21 // 主片的数据端口
#define PIC_SLAVE_CTRL 0xa0 // 从片的控制端口
#define PIC_SLAVE_DATA 0xa1 // 从片的数据端口

#define IDT_ENTRY_COUNT 0x81 // 支持的中断数

#define EFLAGS_IF 0x00000200 // eflags寄存器中的if位为1
#define GET_EFLAGS(EFLAGS_VAR) __asm__ __volatile__("pushfl; popl %0" : "=g"(EFLAGS_VAR))

extern void intr_func0();
extern void intr_func1();
extern void intr_func2();
extern void intr_func3();
extern void intr_func4();
extern void intr_func5();
extern void intr_func6();
extern void intr_func7();
extern void intr_func8();
extern void intr_func9();
extern void intr_func10();
extern void intr_func11();
extern void intr_func12();
extern void intr_func13();
extern void intr_func14();
extern void intr_func15();
extern void intr_func16();
extern void intr_func17();
extern void intr_func18();
extern void intr_func19();
extern void intr_func20();
extern void intr_func21();
extern void intr_func22();
extern void intr_func23();
extern void intr_func24();
extern void intr_func25();
extern void intr_func26();
extern void intr_func27();
extern void intr_func28();
extern void intr_func29();
extern void intr_func30();
extern void intr_func31();
extern void intr_func32();
extern void intr_func33();
extern void intr_func34();
extern void intr_func35();
extern void intr_func36();
extern void intr_func37();
extern void intr_func38();
extern void intr_func39();
extern void intr_func40();
extern void intr_func41();
extern void intr_func42();
extern void intr_func43();
extern void intr_func44();
extern void intr_func45();
extern void intr_func46();
extern void intr_func47();

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
void* intr_offsets[IDT_ENTRY_COUNT]; // 中断处理函数入口地址数组
void* intr_handlers[IDT_ENTRY_COUNT]; // 中断处理函数

// 设置中断描述符
static void set_idt_entry(int index, uint8_t attribute, void *intr_func) {
	idt_entries[index].offset_low = (uint32_t) intr_func & 0x0000ffff;
	idt_entries[index].selector = SELECTOR_KERNEL_CODE; // 内核代码段选择子
	idt_entries[index].reserved = 0;
	idt_entries[index].attribute = attribute;
	idt_entries[index].offset_high = ((uint32_t) intr_func & 0xffff0000) >> 16;
	intr_offsets[index] = intr_func;
}

// 初始化中断描述符表
static void init_idt_entries(void) {
	set_idt_entry(0, IDT_ENTRY_ATTR_DPL0, intr_func0);
	set_idt_entry(1, IDT_ENTRY_ATTR_DPL0, intr_func1);
	set_idt_entry(2, IDT_ENTRY_ATTR_DPL0, intr_func2);
	set_idt_entry(3, IDT_ENTRY_ATTR_DPL0, intr_func3);
	set_idt_entry(4, IDT_ENTRY_ATTR_DPL0, intr_func4);
	set_idt_entry(5, IDT_ENTRY_ATTR_DPL0, intr_func5);
	set_idt_entry(6, IDT_ENTRY_ATTR_DPL0, intr_func6);
	set_idt_entry(7, IDT_ENTRY_ATTR_DPL0, intr_func7);
	set_idt_entry(8, IDT_ENTRY_ATTR_DPL0, intr_func8);
	set_idt_entry(9, IDT_ENTRY_ATTR_DPL0, intr_func9);
	set_idt_entry(10, IDT_ENTRY_ATTR_DPL0, intr_func10);
	set_idt_entry(11, IDT_ENTRY_ATTR_DPL0, intr_func11);
	set_idt_entry(12, IDT_ENTRY_ATTR_DPL0, intr_func12);
	set_idt_entry(13, IDT_ENTRY_ATTR_DPL0, intr_func13);
	set_idt_entry(14, IDT_ENTRY_ATTR_DPL0, intr_func14);
	set_idt_entry(15, IDT_ENTRY_ATTR_DPL0, intr_func15);
	set_idt_entry(16, IDT_ENTRY_ATTR_DPL0, intr_func16);
	set_idt_entry(17, IDT_ENTRY_ATTR_DPL0, intr_func17);
	set_idt_entry(18, IDT_ENTRY_ATTR_DPL0, intr_func18);
	set_idt_entry(19, IDT_ENTRY_ATTR_DPL0, intr_func19);
	set_idt_entry(20, IDT_ENTRY_ATTR_DPL0, intr_func20);
	set_idt_entry(21, IDT_ENTRY_ATTR_DPL0, intr_func21);
	set_idt_entry(22, IDT_ENTRY_ATTR_DPL0, intr_func22);
	set_idt_entry(23, IDT_ENTRY_ATTR_DPL0, intr_func23);
	set_idt_entry(24, IDT_ENTRY_ATTR_DPL0, intr_func24);
	set_idt_entry(25, IDT_ENTRY_ATTR_DPL0, intr_func25);
	set_idt_entry(26, IDT_ENTRY_ATTR_DPL0, intr_func26);
	set_idt_entry(27, IDT_ENTRY_ATTR_DPL0, intr_func27);
	set_idt_entry(28, IDT_ENTRY_ATTR_DPL0, intr_func28);
	set_idt_entry(29, IDT_ENTRY_ATTR_DPL0, intr_func29);
	set_idt_entry(30, IDT_ENTRY_ATTR_DPL0, intr_func30);
	set_idt_entry(31, IDT_ENTRY_ATTR_DPL0, intr_func31);
	set_idt_entry(32, IDT_ENTRY_ATTR_DPL0, intr_func32);
	set_idt_entry(33, IDT_ENTRY_ATTR_DPL0, intr_func33);
	set_idt_entry(34, IDT_ENTRY_ATTR_DPL0, intr_func34);
	set_idt_entry(35, IDT_ENTRY_ATTR_DPL0, intr_func35);
	set_idt_entry(36, IDT_ENTRY_ATTR_DPL0, intr_func36);
	set_idt_entry(37, IDT_ENTRY_ATTR_DPL0, intr_func37);
	set_idt_entry(38, IDT_ENTRY_ATTR_DPL0, intr_func38);
	set_idt_entry(39, IDT_ENTRY_ATTR_DPL0, intr_func39);
	set_idt_entry(40, IDT_ENTRY_ATTR_DPL0, intr_func40);
	set_idt_entry(41, IDT_ENTRY_ATTR_DPL0, intr_func41);
	set_idt_entry(42, IDT_ENTRY_ATTR_DPL0, intr_func42);
	set_idt_entry(43, IDT_ENTRY_ATTR_DPL0, intr_func43);
	set_idt_entry(44, IDT_ENTRY_ATTR_DPL0, intr_func44);
	set_idt_entry(45, IDT_ENTRY_ATTR_DPL0, intr_func45);
	set_idt_entry(46, IDT_ENTRY_ATTR_DPL0, intr_func46);
	set_idt_entry(47, IDT_ENTRY_ATTR_DPL0, intr_func47);
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
	
	outb(PIC_MASTER_DATA, 0xfe);
	outb(PIC_SLAVE_DATA, 0xff);
	
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
	while(1); // 使CPU悬停于此
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
	uint64_t idt_operand = ((sizeof(idt_entries) - 1) | \
		((uint64_t) (uint32_t) idt_entries << 16));
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
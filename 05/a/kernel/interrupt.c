#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "x86.h"
#include "print.h"

#define PIC_MASTER_CTRL 0x20 // 主片的控制端口
#define PIC_MASTER_DATA 0x21 // 主片的数据端口
#define PIC_SLAVE_CTRL 0xa0 // 从片的控制端口
#define PIC_SLAVE_DATA 0xa1 // 从片的数据端口

#define IDT_DESC_COUNT 0x21 // 支持的中断数

#define EFLAGS_IF 0x00000200 // eflags寄存器中的if位为1
#define GET_EFLAGS(EFLAGS_VAR) __asm__ __volatile__("pushfl; popl %0" : "=g"(EFLAGS_VAR))

// 中断门描述符结构体
struct gate_desc {
	uint16_t func_offset_low_word;
	uint16_t selector;
	// 此项为双字计数字段,是门描述符的第4个字节
	// 此项为固定值,不用考虑
	uint8_t dwcount;
	uint8_t attribute;
	uint16_t func_offset_high_word;
};

static struct gate_desc idt[IDT_DESC_COUNT]; // idt是中断描述符表,本质是数组

char *intr_name[IDT_DESC_COUNT]; // 用于保存异常的名字

intr_handler idt_table[IDT_DESC_COUNT]; // 中断处理程序数组

extern intr_handler intr_entry_table[IDT_DESC_COUNT]; // 中断处理函数的入口数组

// 创建中断描述符
static void create_idt_desc(struct gate_desc *gate_desc_ptr, uint8_t attr, intr_handler func) {
	gate_desc_ptr->func_offset_low_word = (uint32_t) func & 0x0000ffff;
	gate_desc_ptr->selector = SELECTOR_KERNEL_CODE;
	gate_desc_ptr->dwcount = 0;
	gate_desc_ptr->attribute = attr;
	gate_desc_ptr->func_offset_high_word = ((uint32_t) func & 0xffff0000) >> 16;
}

// 初始化中断描述符表
static void init_idt_desc(void) {
	for(int i = 0; i < IDT_DESC_COUNT; i++) {
		create_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
	}
	put_str("init_idt_desc done\n");
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
	// 打开主片上IR0,即目前只接受时钟产生的中断
	outb(PIC_MASTER_DATA, 0xfe);
	outb(PIC_SLAVE_DATA, 0xff);
	
	put_str("init_pic done\n");
}

// 通用的中断处理函数,一般用在异常出现时的处理
static void general_intr_handler(uint8_t vector_num) {
	// IRQ7和IRQ15会产生伪中断,无需处理
	// 0x2f是从片8259A上的最后一个IRQ引脚,保留项
	if(vector_num == 0x27 || vector_num == 0x2f) {
		return;
	}
	put_str("int vector : 0x");
	put_int(vector_num);
	put_char('\n');
}

// 初始化一般中断处理函数注册和异常名称注册
static void init_exception(void) {
	for(int i = 0; i < IDT_DESC_COUNT; i++) {
		idt_table[i] = general_intr_handler;
		intr_name[i] = "unknown";
	}
	intr_name[0] = "#DE Divide Error";
	intr_name[1] = "#DB Debug Exception";
	intr_name[2] = "#NMI None Maskable Interrupt";
	intr_name[3] = "#BP Breakpoint Exception";
	intr_name[4] = "#OF Overflow Exception";
	intr_name[5] = "#BR Bound Range Exceeded Exception";
	intr_name[6] = "#UD Invalid Opcode Exception";
	intr_name[7] = "#NM Device Not Available Exception";
	intr_name[8] = "#DF Double Fault Exception";
	intr_name[9] = "#CSO Coprocessor Segment Overrun";
	intr_name[10] = "#TS Invalid TSS Exception";
	intr_name[11] = "#NP Segment Not Present";
	intr_name[12] = "#SS Stack Fault Exception";
	intr_name[13] = "#GP General Protection Exception";
	intr_name[14] = "#PF Page Fault Exception";
	// intr_name[15]是Intel保留项,未使用
	intr_name[16] = "#MF x87 FPU Floating Point Error";
	intr_name[17] = "#AC Alignment Check Exception";
	intr_name[18] = "MC Machine Check Exception";
	intr_name[19] = "#XF SIMD Floating Point Exception";
}

// 初始化中断的所有初始化工作
void init_idt(void) {
	put_str("init_idt start\n");
	init_idt_desc(); // 初始化中断描述符表
	init_exception(); // 初始化异常名并注册中断处理函数
	init_pic(); // 初始化8259A
	
	// 加载idt
	uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t) (uint32_t) idt << 16));
	__asm__ __volatile__("lidt %0" : : "m"(idt_operand));
	put_str("init_idt done\n");
}

// 开中断并返回开中断前的状态
void enable_intr(void) {
	enum intr_status old_status;
	if(INTR_ON == get_intr_status()) {
		old_status = INTR_ON;
	} else {
		old_status = INTR_OFF;
		__asm__ __volatile__("sti");
	}
}

// 关中断并返回关中断前的状态
void disable_intr(void) {
	enum intr_status old_status;
	if(INTR_ON == get_intr_status()) {
		old_status = INTR_ON;
		__asm__ __volatile__("cli" : : : "memory");
	} else {
		old_status = INTR_OFF;
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

// 获取当前中断状态
enum intr_status get_intr_status(void) {
	uint32_t eflags = 0;
	GET_EFLAGS(eflags);
	return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}
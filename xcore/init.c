#include "gdt.h"
#include "interrupt.h"
#include "page.h"

void init_all() {
	init_gdt(); // 初始化GDT
	init_idt(); // 初始化IDT
	init_page(); // 初始化分页
}
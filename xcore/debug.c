#include "print.h"
#include "interrupt.h"

// 打印文件名,行号,函数名,条件,并使程序悬停
void panic_spin(char *filename, int line, const char *func, const char *condition) {
	disable_intr();
	put_str("\n!!!error!!!\n");
	put_str("filename: ");
	put_str(filename);
	put_str("\nline: 0x");
	put_hex(line);
	put_str("\nfunction: ");
	put_str((char *) func);
	put_str("\ncondition: ");
	put_str((char *)condition);
	while(1);
}
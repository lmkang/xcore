[bits 32]
;若CPU自动压入错误码,为了栈中格式统一,这里不做操作
%define ERROR_CODE nop
;如果CPU没有压入错误码,为了栈中格式统一,手动压入一个0
%define ZERO push 0

extern idt_table ;中断处理程序数组

section .data
global intr_entry_table
intr_entry_table:
%macro INTR_VECTOR 2
section .text
intr%1entry:
	%2
	;保存上下文环境
	push ds
	push es
	push fs
	push gs
	pushad
	
	mov al, 0x20 ;中断结束命令EOI
	out 0xa0, al ;向从片发送
	out 0x20, al ;向主片发送
	
	push %1
	call [idt_table + %1 * 4]
	jmp intr_exit
	
section .data
	;存储各个中断入口程序的地址,形成intr_entry_table数组
	dd intr%1entry
%endmacro

section .text
global intr_exit
intr_exit:
	;恢复上下文环境
	add esp, 4
	popad
	pop gs
	pop fs
	pop es
	pop ds
	add esp, 4 ;跳过error_code
	iretd

INTR_VECTOR 0x00, ZERO
INTR_VECTOR 0x01, ZERO
INTR_VECTOR 0x02, ZERO
INTR_VECTOR 0x03, ZERO
INTR_VECTOR 0x04, ZERO
INTR_VECTOR 0x05, ZERO
INTR_VECTOR 0x06, ZERO
INTR_VECTOR 0x07, ZERO
INTR_VECTOR 0x08, ERROR_CODE
INTR_VECTOR 0x09, ZERO
INTR_VECTOR 0x0a, ERROR_CODE
INTR_VECTOR 0x0b, ERROR_CODE
INTR_VECTOR 0x0c, ZERO
INTR_VECTOR 0x0d, ERROR_CODE
INTR_VECTOR 0x0e, ERROR_CODE
INTR_VECTOR 0x0f, ZERO
INTR_VECTOR 0x10, ZERO
INTR_VECTOR 0x11, ERROR_CODE
INTR_VECTOR 0x12, ZERO
INTR_VECTOR 0x13, ZERO
INTR_VECTOR 0x14, ZERO
INTR_VECTOR 0x15, ZERO
INTR_VECTOR 0x16, ZERO
INTR_VECTOR 0x17, ZERO
INTR_VECTOR 0x18, ERROR_CODE
INTR_VECTOR 0x19, ZERO
INTR_VECTOR 0x1a, ERROR_CODE
INTR_VECTOR 0x1b, ERROR_CODE
INTR_VECTOR 0x1c, ZERO
INTR_VECTOR 0x1d, ERROR_CODE
INTR_VECTOR 0x1e, ERROR_CODE
INTR_VECTOR 0x1f, ZERO
INTR_VECTOR 0x20, ZERO ;时钟中断对应的入口
INTR_VECTOR 0x21, ZERO ;键盘中断对应的入口
INTR_VECTOR 0x22, ZERO ;级联用的
INTR_VECTOR 0x23, ZERO ;串口2对应的入口
INTR_VECTOR 0x24, ZERO ;串口1对应的入口
INTR_VECTOR 0x25, ZERO ;并口2对应的入口
INTR_VECTOR 0x26, ZERO ;软盘对应的入口
INTR_VECTOR 0x27, ZERO ;并口1对应的入口
INTR_VECTOR 0x28, ZERO ;实时时钟对应的入口
INTR_VECTOR 0x29, ZERO ;重定向
INTR_VECTOR 0x2a, ZERO ;保留
INTR_VECTOR 0x2b, ZERO ;保留
INTR_VECTOR 0x2c, ZERO ;PS/2鼠标
INTR_VECTOR 0x2d, ZERO ;FPU浮点单元异常
INTR_VECTOR 0x2e, ZERO ;硬盘
INTR_VECTOR 0x2f, ZERO ;保留
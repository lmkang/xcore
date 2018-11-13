[bits 32]
%define ERRCODE nop
%define NOERRCODE push 0
extern intr_handlers

%macro INTR_VECTOR 2
	section .text
	global intr_func%1
	intr_func%1:
		%2
		push ds
		push es
		push fs
		push gs
		pushad
		
		mov al, 0x20 ;中断结束命令EOI
		out 0xa0, al ;向从片发送
		out 0x20, al ;向主片发送
		
		push %1
		call [intr_handlers + %1 * 4]
		
		jmp intr_exit
%endmacro

section .text
global intr_exit
intr_exit:
	add esp, 4
	popad
	pop gs
	pop fs
	pop es
	pop ds
	add esp, 4 ;跳过error_code
	iretd

INTR_VECTOR 0, NOERRCODE
INTR_VECTOR 1, NOERRCODE
INTR_VECTOR 2, NOERRCODE
INTR_VECTOR 3, NOERRCODE
INTR_VECTOR 4, NOERRCODE
INTR_VECTOR 5, NOERRCODE
INTR_VECTOR 6, NOERRCODE
INTR_VECTOR 7, NOERRCODE
INTR_VECTOR 8, ERRCODE
INTR_VECTOR 9, NOERRCODE
INTR_VECTOR 10, ERRCODE
INTR_VECTOR 11, ERRCODE
INTR_VECTOR 12, NOERRCODE
INTR_VECTOR 13, ERRCODE
INTR_VECTOR 14, ERRCODE
INTR_VECTOR 15, NOERRCODE
INTR_VECTOR 16, NOERRCODE
INTR_VECTOR 17, ERRCODE
INTR_VECTOR 18, NOERRCODE
INTR_VECTOR 19, NOERRCODE
INTR_VECTOR 20, NOERRCODE
INTR_VECTOR 21, NOERRCODE
INTR_VECTOR 22, NOERRCODE
INTR_VECTOR 23, NOERRCODE
INTR_VECTOR 24, ERRCODE
INTR_VECTOR 25, NOERRCODE
INTR_VECTOR 26, ERRCODE
INTR_VECTOR 27, ERRCODE
INTR_VECTOR 28, NOERRCODE
INTR_VECTOR 29, ERRCODE
INTR_VECTOR 30, ERRCODE
INTR_VECTOR 31, NOERRCODE
INTR_VECTOR 32, NOERRCODE ;时钟中断对应的入口
INTR_VECTOR 33, NOERRCODE ;键盘中断对应的入口
INTR_VECTOR 34, NOERRCODE ;级联用的
INTR_VECTOR 35, NOERRCODE ;串口2对应的入口
INTR_VECTOR 36, NOERRCODE ;串口1对应的入口
INTR_VECTOR 37, NOERRCODE ;并口2对应的入口
INTR_VECTOR 38, NOERRCODE ;软盘对应的入口
INTR_VECTOR 39, NOERRCODE ;并口1对应的入口
INTR_VECTOR 40, NOERRCODE ;实时时钟对应的入口
INTR_VECTOR 41, NOERRCODE ;重定向
INTR_VECTOR 42, NOERRCODE ;保留
INTR_VECTOR 43, NOERRCODE ;保留
INTR_VECTOR 44, NOERRCODE ;PS/2鼠标
INTR_VECTOR 45, NOERRCODE ;FPU浮点单元异常
INTR_VECTOR 46, NOERRCODE ;硬盘
INTR_VECTOR 47, NOERRCODE ;保留

[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
	;1 保存上下文环境
	push 0 ;压入0,使栈中格式统一
	push ds
	push es
	push fs
	push gs
	pushad
	push 0x80 ;为了保持统一的栈格式
	;2 为系统调用子功能传入参数
	push edx ;第3个参数
	push ecx ;第2个参数
	push ebx ;第1个参数
	;3 调用子功能处理函数
	call [syscall_table + eax * 4]
	add esp, 12 ;跨过上面的3个参数
	;4 将call调用后的返回值存入当前内核栈中eax的位置
	mov [esp + 8 * 4], eax
	jmp intr_exit ;intr_exit返回,恢复上下文
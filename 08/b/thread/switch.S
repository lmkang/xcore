[bits 32]
section .text
global switch_to
switch_to:
	;栈中此处是返回地址
	push esi
	push edi
	push ebx
	push ebp
	
	;备份当前线程的环境
	;得到栈中的参数cur_thread
	mov eax, [esp + 20]
	;保存栈顶指针esp,task_struct的self_kstack字段
	mov [eax], esp
	
	;恢复下一个线程的环境
	;得到栈中的参数next_task
	mov eax, [esp + 24]
	;PCB的第一个成员是self_kstack
	mov esp, [eax]
	pop ebp
	pop ebx
	pop edi
	pop esi
	ret
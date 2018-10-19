[bits 32]
section .text
global switch_to
switch_to:
	;栈中此处是返回地址
	push esi
	push edi
	push ebx
	push ebp
	;得到栈中的参数cur_thread, cur_thread = [esp + 20]
	;保存栈顶指针esp, task_struct的self_kstack字段
	mov eax, [esp + 20]
	;self_kstack在task_struct中的偏移为0
	;所以直接往thread开头处存4字节即可
	mov [eax], esp
	;以上是备份当前线程环境,下面是恢复下一线程的环境
	;得到栈中的参数next_task, next_task = [esp + 24]
	mov eax, [esp + 24]
	;PCB的第一个成员是self_kstack成员
	;它用来记录0级栈顶指针,被换上CPU时用来恢复0级栈
	;0级栈中保存了进程或线程所有信息,包括3级栈指针
	mov esp, [eax]
	pop ebp
	pop ebx
	pop edi
	pop esi
	;返回到上面switch_to下面的那句注释的返回地址
	;未由中断进入,第一次执行时会返回到kernel_thread
	ret
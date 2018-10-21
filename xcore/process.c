#include "thread.h"
#include "global.h"

// idtasm.S文件的intr_exit
extern void intr_exit(void);

// 构建用户进程初始上下文信息
void start_process(void *filename) {
	struct task_struct *cur_thread = current_thread();
	cur_thread->self_kstack += sizeof(struct thread_stack);
	struct intr_stack *proc_stack = (struct intr_stack*) cur_thread->self_kstack;
	proc_stack->edi = 0;
	proc_stack->esi = 0;
	proc_stack->ebp = 0;
	proc_stack->esp_dummy = 0;
	proc_stack->ebx = 0;
	proc_stack->edx = 0;
	proc_stack->ecx = 0;
	proc_stack->eax = 0;
	proc_stack->gs = 0; // 用户态用不上,直接初始为0
	proc_stack->ds = SELECTOR_USER_DATA;
	proc_stack->es = SELECTOR_USER_DATA;
	proc_stack->fs = SELECTOR_USER_DATA;
	proc_stack->eip = filename;
	proc_stack->cs = SELECTOR_USER_CODE;
	proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
	proc_stack->esp = (void*) ((uint32_t) get_a_page());
}






















































































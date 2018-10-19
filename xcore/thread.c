#include "thread.h"
#include "global.h"
#include "string.h"
#include "memory.h"

// 由kernel_thread去执行func(func_arg)
static void kernel_thread(thread_func *func, void *func_arg) {
	func(func_arg);
}

// 创建线程
void thread_create(struct task_struct *pthread, thread_func func, void *func_arg) {
	// 预留中断栈的空间
	pthread->self_kstack -= sizeof(struct intr_stack);
	// 预留线程栈的空间
	pthread->self_kstack -= sizeof(struct thread_stack);
	struct thread_stack *kthread_stack = (struct thread_stack*) pthread->self_kstack;
	kthread_stack->eip = kernel_thread;
	kthread_stack->func = func;
	kthread_stack->func_arg = func_arg;
	kthread_stack->ebp = 0;
	kthread_stack->ebx = 0;
	kthread_stack->esi = 0;
	kthread_stack->edi = 0;
}

// 初始化线程基本信息
void init_thread(struct task_struct *pthread, char *name, uint8_t priority) {
	memset(pthread, 0, sizeof(*pthread));
	strcpy(pthread->name, name);
	pthread->status = TASK_RUNNING;
	pthread->priority = priority;
	// self_kstack是线程在内核态下使用的栈顶地址
	pthread->self_kstack = (uint32_t*) ((uint32_t) pthread + PAGE_SIZE);
	pthread->stack_magic = 0x19940625; // 自定义的魔数
}

// 开始执行线程
struct task_struct *thread_start(char *name, uint8_t priority, \
	thread_func func, void *func_arg) {
	// PCB都位于内核空间,包括用户进程的PCB也是在内核空间
	struct task_struct *thread = kmalloc(1);
	init_thread(thread, name, priority);
	thread_create(thread, func, func_arg);
	__asm__ __volatile__(" \
		movl %0, %%esp; \
		pop %%ebp; \
		pop %%ebx; \
		pop %%edi; \
		pop %%esi; \
		ret" \
		: : "g"(thread->self_kstack) : "memory" \
	);
	return thread;
}











































































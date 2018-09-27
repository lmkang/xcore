#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"

#define PAGE_SIZE 4096

struct task_struct *main_thread; // 主线程PCB
struct list thread_ready_list; //就绪队列
struct list thread_all_list; // 所有任务队列
static struct list_ele *thread_tag; // 用于保存队列中的线程节点

extern void switch_to(struct task_struct *cur_task, struct task_struct *next_task);

// 获取当前线程PCB指针
struct task_struct * current_thread() {
	uint32_t esp;
	__asm__("mov %%esp, %0" : "=g"(esp));
	// 取esp整数部分,即PCB起始地址
	return (struct task_struct*) (esp & 0xfffff000);
}

// 由kernel_thread去执行func(func_arg)
static void kernel_thread(thread_func *func, void *func_arg) {
	// 执行func前要开中断,避免后面的时钟中断被屏蔽而无法调度其他线程
	enable_intr();
	func(func_arg);
}

// 初始化线程栈thread_stack
// 将待执行的函数和参数放到thread_stack中相应的位置
void thread_create(struct task_struct *pthread, thread_func func, void *func_arg) {
	// 先预留中断使用栈的空间
	pthread->self_kstack -= sizeof(struct intr_stack);
	// 再留出线程栈的空间
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
void init_thread(struct task_struct *pthread, char *name, int priority) {
	memset(pthread, 0, sizeof(*pthread));
	strcpy(pthread->name, name);
	// 由于把main函数也封装成一个线程,并且它是一直运行的,故直接设为TASK_RUNNING
	if(pthread == main_thread) {
		pthread->status = TASK_RUNNING;
	} else {
		pthread->status = TASK_READY;
	}
	// self_kstack是线程自己在内核态下使用的栈顶地址
	pthread->self_kstack = (uint32_t*) ((uint32_t) pthread + PAGE_SIZE);
	pthread->priority = priority;
	pthread->ticks = priority;
	pthread->elapsed_ticks = 0;
	pthread->page_vaddr = NULL;
	pthread->stack_magic = 0x19940625; // 自定义的魔数
}

// 创建优先级为priority的线程,线程名为name
// 线程所执行的函数是func(func_arg)
struct task_struct * thread_start(char *name, int priority, thread_func func, void *func_arg) {
	// PCB都位于内核空间,包括用户进程的PCB也是在内核空间
	struct task_struct *thread = alloc_kernel_page(1);
	init_thread(thread, name, priority);
	thread_create(thread, func, func_arg);
	// 确保之前不在队列中
	ASSERT(!has_ele(&thread_ready_list, &thread->general_tag));
	// 加入就绪队列
	list_append(&thread_ready_list, &thread->general_tag);
	// 确保之前不在队列中
	ASSERT(!has_ele(&thread_all_list, &thread->all_list_tag));
	// 加入全部线程队列
	list_append(&thread_all_list, &thread->all_list_tag);
	return thread;
}

// 将kernel中的main函数完善为主线程
static void create_main_thread(void) {
	// 因为main线程早已运行
	// 在loader.S中进入内核时,esp=0xc009f000
	// 因此main线程的PCB地址为0xc009f000
	main_thread = current_thread();
	init_thread(main_thread, "main", 31);
	// main函数是当前线程,当前线程不在thread_ready_list中
	// 因此只需加入thread_all_list
	ASSERT(!has_ele(&thread_all_list, &main_thread->all_list_tag));
	list_append(&thread_all_list, &main_thread->all_list_tag);
}

// 实现任务调度
void schedule() {
	
}












































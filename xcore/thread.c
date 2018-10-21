#include "thread.h"
#include "global.h"
#include "string.h"
#include "memory.h"
#include "debug.h"
#include "interrupt.h"
#include "print.h"

struct task_struct *main_thread; // 主线程PCB
struct list thread_ready_list; // 就绪队列
struct list thread_all_list; // 所有任务队列
static struct list_ele *thread_tag; // 用于保存队列中的线程节点

extern void switch_to(struct task_struct *cur_task, struct task_struct *next_task);

struct task_struct *current_thread(void) {
	uint32_t esp;
	__asm__ __volatile__("mov %%esp, %0" : "=g"(esp));
	// 取esp整数部分,即PCB起始地址
	return (struct task_struct*) (esp & 0xfffff000);
}

// 由kernel_thread去执行func(func_arg)
static void kernel_thread(thread_func *func, void *func_arg) {
	// 执行func前要开中断,避免时钟中断被屏蔽而无法调度其他进程
	enable_intr();
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
	// main方法被封装为主线程
	if(pthread == main_thread) {
		pthread->status = TASK_RUNNING;
	} else {
		pthread->status = TASK_READY;
	}
	// self_kstack是线程在内核态下使用的栈顶地址
	pthread->self_kstack = (uint32_t*) ((uint32_t) pthread + PAGE_SIZE);
	pthread->priority = priority;
	pthread->ticks = priority;
	pthread->elapsed_ticks = 0;
	pthread->pgdir = NULL;
	pthread->stack_magic = 0x19940625; // 自定义的魔数
}

// 开始执行线程
struct task_struct *thread_start(char *name, uint8_t priority, \
	thread_func func, void *func_arg) {
	// PCB都位于内核空间,包括用户进程的PCB也是在内核空间
	struct task_struct *thread = get_kernel_pages(1);
	init_thread(thread, name, priority);
	thread_create(thread, func, func_arg);
	
	// 确保之前不在就绪队列中
	ASSERT(!list_find(&thread_ready_list, &thread->general_tag));
	// 加入就绪队列
	list_append(&thread_ready_list, &thread->general_tag);
	// 确保之前不在所有线程队列中
	ASSERT(!list_find(&thread_all_list, &thread->all_list_tag));
	// 加入所有线程的队列
	list_append(&thread_all_list, &thread->all_list_tag);
	
	return thread;
}

// 将kernel中的kmain函数封装为主线程
static void create_main_thread(void) {
	main_thread = current_thread();
	init_thread(main_thread, "main", 31);
	// kmain函数是当前线程,不需要加入就绪队列
	ASSERT(!list_find(&thread_all_list, &main_thread->all_list_tag));
	list_append(&thread_all_list, &main_thread->all_list_tag);
}

// 实现任务调度
void schedule(void) {
	ASSERT(get_intr_status() == INTR_OFF);
	struct task_struct *cur_thread = current_thread();
	if(cur_thread->status == TASK_RUNNING) {
		// 若此线程只是CPU时间片到了,将其加入就绪队列队尾
		ASSERT(!list_find(&thread_ready_list, &cur_thread->general_tag));
		list_append(&thread_ready_list, &cur_thread->general_tag);
		cur_thread->ticks = cur_thread->priority;
		cur_thread->status = TASK_READY;
	} else {
		// 若此线程需要某事件发生后才能继续上CPU运行,
		// 不需要将其加入队列,因为当前线程不在就绪队列中
	}
	ASSERT(!list_empty(&thread_ready_list));
	thread_tag = NULL;
	// 将thread_ready_list队列中的第一个就绪线程弹出,准备调度上CPU
	thread_tag = list_pop(&thread_ready_list);
	struct task_struct *next_task = ELE2ENTRY(struct task_struct, general_tag, thread_tag);
	next_task->status = TASK_RUNNING;
	switch_to(cur_thread, next_task);
}

// 当前线程将自己阻塞,设置其状态为status
void thread_block(enum task_status status) {
	enum intr_status old_status = get_intr_status();
	disable_intr();
	// status取值为TASK_BLOCKED,TASK_WAITING,TASK_HANGING
	ASSERT((status == TASK_BLOCKED) \
		|| (status == TASK_WAITING) || (status == TASK_HANGING));
	struct task_struct *cur_thread = current_thread();
	cur_thread->status = status;
	schedule();
	set_intr_status(old_status);
}

// 将线程pthread解除阻塞
void thread_unblock(struct task_struct *pthread) {
	enum intr_status old_status = get_intr_status();
	disable_intr();
	ASSERT((pthread->status == TASK_BLOCKED) \
		|| (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING));
	if(pthread->status != TASK_READY) {
		ASSERT(!list_find(&thread_ready_list, &pthread->general_tag));
		if(list_find(&thread_ready_list, &pthread->general_tag)) {
			PANIC("thread_unblock : blocked thread in thread_ready_list!\n");
		}
		// 放到队列最前面,使其尽快得到调度
		pthread->status = TASK_READY;
		list_push(&thread_ready_list, &pthread->general_tag);
	}
	set_intr_status(old_status);
}

// 初始化线程环境
void thread_init(void) {
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	create_main_thread();
	printk("thread_init done\n");
}




















































#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "interrupt.h"
#include "interrupt.h"
#include "debug.h"
#include "list.h"
#include "process.h"
#include "string.h"
#include "sync.h"
#include "print.h"

#define PAGE_SIZE 4096

struct lock pid_lock; // 分配pid锁
struct task_struct *main_thread; // 主线程PCB
struct list thread_ready_list; //就绪队列
struct list thread_all_list; // 所有任务队列
static struct list_ele *thread_tag; // 用于保存队列中的线程节点

struct task_struct *idle_thread; // idle线程

extern void switch_to(struct task_struct *cur_task, struct task_struct *next_task);

// 获取当前线程PCB指针
struct task_struct * current_thread(void) {
	uint32_t esp;
	__asm__("mov %%esp, %0" : "=g"(esp));
	// 取esp整数部分,即PCB起始地址
	return (struct task_struct*) (esp & 0xfffff000);
}

// idle线程func
static void idle_func(__attribute__((unused))void *arg) {
	while(1) {
		thread_block(TASK_BLOCKED);
		// 执行hlt时必须要保证处在开中断的情况下
		__asm__ __volatile__("sti; hlt" : : : "memory");
	}
}

// 由kernel_thread去执行func(func_arg)
static void kernel_thread(thread_func *func, void *func_arg) {
	// 执行func前要开中断,避免后面的时钟中断被屏蔽而无法调度其他线程
	enable_intr();
	func(func_arg);
}

// 分配pid
static uint32_t alloc_pid(void) {
	static uint32_t next_pid = 0;
	lock_acquire(&pid_lock);
	++next_pid;
	lock_release(&pid_lock);
	return next_pid;
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
	pthread->pid = alloc_pid();
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

// 主动让出CPU,换其他线程运行
void thread_yield(void) {
	struct task_struct *cur_thread = current_thread();
	enum intr_status old_status = get_intr_status();
	disable_intr();
	ASSERT(!has_ele(&thread_ready_list, &cur_thread->general_tag));
	cur_thread->status = TASK_READY;
	list_append(&thread_ready_list, &cur_thread->general_tag);
	schedule();
	set_intr_status(old_status);
}

// 初始化线程环境
void thread_init(void) {
	put_str("thread_init start\n");
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	lock_init(&pid_lock);
	create_main_thread();
	idle_thread = thread_start("idle", 10, idle_func, NULL);
	put_str("thread_init done\n");
}

// 实现任务调度
void schedule(void) {
	ASSERT(get_intr_status() == INTR_OFF);
	struct task_struct *cur_thread = current_thread();
	if(cur_thread->status == TASK_RUNNING) {
		// 若此线程只是时间片到了,将其加入到就绪队列尾
		ASSERT(!has_ele(&thread_ready_list, &cur_thread->general_tag));
		list_append(&thread_ready_list, &cur_thread->general_tag);
		cur_thread->ticks = cur_thread->priority;
		cur_thread->status = TASK_READY;
	} else {
		// 若此线程需要某事件发生后才能继续上CPU运行
		// 不需要将其加入队列,因为当前线程不在就绪队列
	}
	// 如果就绪队列中没有可运行的任务,则唤醒idle
	if(list_empty(&thread_ready_list)) {
		thread_unblock(idle_thread);
	}
	ASSERT(!list_empty(&thread_ready_list));
	thread_tag = NULL; // 清空thread_tag
	// 将thread_ready_list队列中的第一个就绪线程弹出,将其调度上CPU
	thread_tag = list_pop(&thread_ready_list);
	struct task_struct *next_task = ele2entry(struct task_struct, general_tag, thread_tag);
	next_task->status = TASK_RUNNING;
	process_activate(next_task); // 激活任务页表等
	switch_to(cur_thread, next_task);
}

// 当前线程将自己阻塞,将状态置为status
void thread_block(enum task_status status) {
	// 只有status取值为TASK_BLOCKED,TASK_WAITING,TASK_HANGING,才不会被调度
	ASSERT((status == TASK_BLOCKED) || (status == TASK_WAITING) || (status == TASK_HANGING));
	enum intr_status old_status = get_intr_status();
	disable_intr();
	struct task_struct *cur_thread = current_thread();
	cur_thread->status = status;
	schedule();
	set_intr_status(old_status);
}

// 将线程pthread解除阻塞
void thread_unblock(struct task_struct *pthread) {
	ASSERT((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) 
		|| (pthread->status == TASK_HANGING));
	enum intr_status old_status = get_intr_status();
	disable_intr();
	if(pthread->status != TASK_READY) {
		ASSERT(!has_ele(&thread_ready_list, &pthread->general_tag));
		if(has_ele(&thread_ready_list, &pthread->general_tag)) {
			PANIC("thread_unblock: blocked thread in ready_list\n");
		}
		// 放在队列的最前面,使其尽快得到调度
		pthread->status = TASK_READY;
		list_push(&thread_ready_list, &pthread->general_tag);
	}
	set_intr_status(old_status);
}
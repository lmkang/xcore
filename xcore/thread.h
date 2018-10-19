#ifndef __THREAD_H
#define __THREAD_H

#include "types.h"
#include "list.h"

// 线程函数类型
typedef void thread_func(void *);

// 进程或线程的状态
enum task_status {
	TASK_RUNNING,
	TASK_READY,
	TASK_BLOCKED,
	TASK_WAITING,
	TASK_HANGING,
	TASK_DIED
};

// 中断栈
// 中断发生时保护上下文环境
// 位置固定,在页的最顶端
struct intr_stack {
	uint32_t vec_no; // 中断向量号
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	// 虽然pushad会把esp压入,但esp是不断变化的,所以会被popad忽略
	uint32_t esp_dummy;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;
	// 以下是CPU从低特权级进入高特权级时压入
	uint32_t err_code; // err_code在eip之后
	void (*eip) (void);
	uint32_t cs;
	uint32_t eflags;
	void *esp;
	uint32_t ss;
};

// 线程栈
// 用于存储线程中待执行的函数,位置不固定
struct thread_stack {
	uint32_t ebp;
	uint32_t ebx;
	uint32_t edi;
	uint32_t esi;
	// 线程第一次执行时,eip指向待调用函数kernel_thread
	// 其他时候,eip指向switch_to的返回地址
	void (*eip) (thread_func *func, void *func_arg);
	// 以下仅供第一次被调度上CPU时使用
	void (*unused_retaddr); // 参数unused_retaddr只为占位置充数为返回地址
	thread_func *func; // 用kernel_thread所调用的函数
	void *func_arg; // func函数的参数
};

// 进程或线程的PCB(程序控制块)
struct task_struct {
	uint32_t *self_kstack; // 各内核线程都用自己的内核栈
	enum task_status status;
	char name[16];
	uint8_t priority; // 线程优先级
	uint8_t ticks; // 时钟嘀嗒数
	uint32_t elapsed_ticks; // 任务已执行的时钟嘀嗒数
	struct list_ele general_tag; // 线程在一般队列中的节点
	struct list_ele all_list_tag; // 线程在thread_all_list中的节点
	uint32_t *pgdir; // 进程的页目录虚拟地址,如果是线程则为NULL
	uint32_t stack_magic; // 魔数,用于检测栈的溢出
};

struct task_struct *current_thread(void);

struct task_struct *thread_start(char *name, uint8_t priority, \
	thread_func func, void *func_arg);

void schedule(void);

void thread_init(void);

#endif
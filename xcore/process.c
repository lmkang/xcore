#include "thread.h"
#include "global.h"
#include "memory.h"
#include "debug.h"
#include "print.h"
#include "string.h"
#include "descriptor.h"
#include "interrupt.h"

extern struct list thread_ready_list; // 就绪队列
extern struct list thread_all_list; // 所有任务队列

// 构建用户进程初始上下文信息
void start_process(void *filename) {
	ASSERT(filename != NULL);
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
	proc_stack->esp = (void*) ((uint32_t) get_pages(USER_STACK3_VADDR, 1) + PAGE_SIZE);
	proc_stack->ss = SELECTOR_USER_DATA;
	__asm__ __volatile__(" \
		movl %0, %%esp; \
		jmp intr_exit" \
		: : "g"(proc_stack) : "memory" \
	);
}

// 激活页表
void pgdir_activate(struct task_struct *pthread) {
	ASSERT(pthread != NULL);
	// 执行此函数时,当前任务可能是线程,
	// 如果不重新安装页表的话,线程就会使用上一个进程的页表了
	uint32_t pgdir = V2P((uint32_t) pgd_kern);
	if(pthread->pgdir != NULL) {
		pgdir = kern_v2p((uint32_t) pthread->pgdir);
	}
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(pgdir));
}

// 激活线程或进程的页表,更新TSS中的esp0为进程的特权级0的栈
void process_activate(struct task_struct *pthread) {
	ASSERT(pthread != NULL);
	// 激活页表
	pgdir_activate(pthread);
	// 内核线程特权级本身就是0,
	// 处理器进入中断时并不会从TSS中获取0特权级栈地址,
	// 故不需要更新esp0
	if(pthread->pgdir != NULL) {
		// 更新该进程的esp0,用于此进程被中断时保留上下文环境
		update_tss_esp(pthread);
	}
}

// 创建页目录表,复制内核空间的页表
uint32_t *create_pgdir(void) {
	// 用户进程的页表不能让用户直接访问到,故在内核空间申请
	uint32_t *pgdir_vaddr = get_kernel_pages(1);
	if(pgdir_vaddr == NULL) {
		console_printk("create_pgdir : get_kernel_pages failed!\n");
		return NULL;
	}
	// 复制页表
	uint32_t pgd_index = GET_PGD_INDEX(KERNEL_OFFSET);
	memcpy((uint32_t*) &pgdir_vaddr[pgd_index], \
		(uint32_t*) &pgd_kern[pgd_index], 1024);
	// 返回页目录地址
	return pgdir_vaddr;
}

// 创建用户进程虚拟地址位图
void create_user_vaddr_bitmap(struct task_struct *uprog) {
	uprog->prog_vaddr.vaddr_start = USER_VADDR_START;
	uint32_t btmp_len = (KERNEL_OFFSET - USER_VADDR_START) / (PAGE_SIZE * 8);
	uprog->prog_vaddr.vaddr_btmp.byte_len = btmp_len;
	uint32_t btmp_pg_count = DIV_ROUND_UP( \
		(KERNEL_OFFSET - USER_VADDR_START) / (PAGE_SIZE * 8), PAGE_SIZE);
	uprog->prog_vaddr.vaddr_btmp.bits = get_kernel_pages(btmp_pg_count);
	init_bitmap(&uprog->prog_vaddr.vaddr_btmp);
}

// 创建用户进程
void process_execute(void *filename, char *name) {
	// PCB由内核来维护,故在内核空间申请
	struct task_struct *thread = get_kernel_pages(1);
	init_thread(thread, name, USER_DEFAULT_PRIORITY);
	create_user_vaddr_bitmap(thread);
	thread_create(thread, start_process, filename);
	thread->pgdir = create_pgdir();
	// 放入就绪队列和全部队列
	enum intr_status old_status = get_intr_status();
	disable_intr();
	ASSERT(!list_find(&thread_ready_list, &thread->general_tag));
	list_append(&thread_ready_list, &thread->general_tag);
	ASSERT(!list_find(&thread_all_list, &thread->all_list_tag));
	list_append(&thread_all_list, &thread->all_list_tag);
	set_intr_status(old_status);
}












































































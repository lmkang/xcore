#include "process.h"
#include "console.h"
#include "global.h"
#include "bitmap.h"
#include "interrupt.h"
#include "list.h"
#include "debug.h"
#include "tss.h"
#include "memory.h"
#include "string.h"

#define PAGE_SIZE 4096

extern void intr_exit(void);
extern struct list thread_ready_list; //就绪队列
extern struct list thread_all_list; // 所有任务队列

// 构建用户进程初始上下文信息
void process_start(void *filename) {
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
	proc_stack->gs = 0;
	proc_stack->ds = SELECTOR_USER_DATA;
	proc_stack->es = SELECTOR_USER_DATA;
	proc_stack->fs = SELECTOR_USER_DATA;
	proc_stack->eip = filename;
	proc_stack->cs = SELECTOR_USER_CODE;
	proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
	proc_stack->esp = (void*) ((uint32_t) get_a_page(PF_USER, USER_STACK3_VADDR) + PAGE_SIZE);
	proc_stack->ss = SELECTOR_USER_DATA;
	__asm__ __volatile__("movl %0, %%esp; jmp intr_exit" : : "g"(proc_stack) : "memory");
}

// 激活页表
void page_dir_activate(struct task_struct *pthread) {
	// 执行此函数时,当前任务可能是线程
	// 之所以对线程也要重新安装页表,原因是上一次被调度的可能是进程
	// 否则不恢复页表的话,线程就会使用进程的页表了
	// 若为内核线程,需要重新填充页表的0x100000
	uint32_t pgdir_paddr = 0x100000;
	if(pthread->page_vaddr != NULL) {
		pgdir_paddr = addr_v2p((uint32_t) pthread->page_vaddr);
	}
	// 更新页目录寄存器cr3,使新页表生效
	__asm__ __volatile__("movl %0, %%cr3" : : "r"(pgdir_paddr) : "memory");
}

// 激活线程或进程的页表,更新TSS中的esp0为进程的特权级0的栈
void process_activate(struct task_struct *pthread) {
	ASSERT(pthread != NULL);
	page_dir_activate(pthread);
	// 内核线程特权级本身就是0,处理器进入中断不会从TSS中获取0特权级栈地址
	// 故不需要更新esp0
	if(pthread->page_vaddr) {
		update_tss_esp(pthread);
	}
}

// 创建页目录表,将当前页表的表示内核空间的pde复制
// 成功返回页目录的虚拟地址,否则返回-1
uint32_t * create_page_dir(void) {
	// 用户进程的页表不能让用户直接访问到
	// 所以在内核空间申请
	uint32_t *pgdir_vaddr = alloc_kernel_page(1);
	if(pgdir_vaddr == NULL) {
		console_put_str("create_page_dir: alloc_kernel_page failed!");
		return NULL;
	}
	// 1 先复制页表
	// pgdir_vaddr + 0x300 * 4是内核页目录的第768项
	memcpy((uint32_t*) ((uint32_t) pgdir_vaddr + 0x300 * 4), (uint32_t*) (0xfffff000 + 0x300 * 4), 1024);
	// 2 更新页目录地址
	uint32_t new_pgdir_paddr = addr_v2p((uint32_t) pgdir_vaddr);
	// 页目录地址是存入在页目录的最后一项
	// 更新页目录地址为新页目录的物理地址
	pgdir_vaddr[1023] = new_pgdir_paddr | PG_US_U | PG_RW_W | PG_P_1;
	return pgdir_vaddr;
}

// 创建用户进程虚拟地址位图
void create_user_vaddr_bitmap(struct task_struct *user_task) {
	user_task->userprog_vaddr.vaddr_start = USER_VADDR_START;
	uint32_t btmp_page_count = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PAGE_SIZE / 8, PAGE_SIZE);
	user_task->userprog_vaddr.vaddr_btmp.bits = alloc_kernel_page(btmp_page_count);
	user_task->userprog_vaddr.vaddr_btmp.byte_len = (0xc0000000 - USER_VADDR_START) / PAGE_SIZE / 8;
	init_bitmap(&user_task->userprog_vaddr.vaddr_btmp);
}

// 创建用户进程
void process_execute(void *filename, char *name) {
	// PCB内核的数据结构,由内核来维护进程信息
	// 因此要在内核内存池申请
	struct task_struct *pthread = alloc_kernel_page(1);
	init_thread(pthread, name, DEFAULT_PRIORITY);
	create_user_vaddr_bitmap(pthread);
	thread_create(pthread, process_start, filename);
	pthread->page_vaddr = create_page_dir();
	init_block_desc(pthread->user_block_desc);
	
	enum intr_status old_status = get_intr_status();
	disable_intr();
	ASSERT(!has_ele(&thread_ready_list, &pthread->general_tag));
	list_append(&thread_ready_list, &pthread->general_tag);
	ASSERT(!has_ele(&thread_all_list, &pthread->all_list_tag));
	list_append(&thread_all_list, &pthread->all_list_tag);
	set_intr_status(old_status);
}
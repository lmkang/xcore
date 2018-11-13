#include "memory.h"
#include "thread.h"
#include "string.h"
#include "global.h"
#include "process.h"
#include "file.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"

// 文件表
extern struct file file_table[MAX_FILE_OPEN];

extern struct list thread_ready_list;
extern struct list thread_all_list;

// at idt.S
extern void intr_exit(void);

// 将父进程的PCB复制给子进程
static void copy_pcb(struct task_struct *child_thread, \
	struct task_struct *parent_thread) {
	// 1 复制PCB的整个页,包括进程PCB和特权0级的栈,里面包含返回地址
	memcpy(child_thread, parent_thread, PAGE_SIZE);
	// 修改子进程PCB
	child_thread->pid = fork_pid();
	child_thread->elapsed_ticks = 0;
	child_thread->status = TASK_READY;
	child_thread->ticks = child_thread->priority;
	child_thread->parent_pid = parent_thread->pid;
	child_thread->general_tag.prev = NULL;
	child_thread->general_tag.next = NULL;
	child_thread->all_list_tag.prev = NULL;
	child_thread->all_list_tag.next = NULL;
	init_block_desc(child_thread->prog_block_descs);
	// 2 复制父进程的虚拟地址池的位图
	uint32_t btmp_page_count = \
		DIV_ROUND_UP((KERNEL_OFFSET - USER_VADDR_START) / PAGE_SIZE / 8, PAGE_SIZE);
	void *vaddr_btmp = get_kernel_pages(btmp_page_count);
	// 此时child_thread->prog_vaddr.vaddr_btmp.bits还是指向父进程虚拟地址的位图地址
	// 下面将child_thread->prog_vaddr.vaddr_btmp.bits指向自己的位图vaddr_btmp
	memcpy(vaddr_btmp, child_thread->prog_vaddr.vaddr_btmp.bits, btmp_page_count * PAGE_SIZE);
	child_thread->prog_vaddr.vaddr_btmp.bits = vaddr_btmp;
	// 调试用
	ASSERT(strlen(child_thread->name) < 11);
	strcat(child_thread->name, "_fork");
}

// 复制子进程的进程体(代码和数据)及用户栈
static void copy_prog_body(struct task_struct *child_thread, \
	struct task_struct *parent_thread, void *buf) {
	uint8_t *vaddr_btmp = parent_thread->prog_vaddr.vaddr_btmp.bits;
	uint32_t byte_len = parent_thread->prog_vaddr.vaddr_btmp.byte_len;
	uint32_t vaddr_start = parent_thread->prog_vaddr.vaddr_start;
	uint32_t byte_index = 0;
	uint32_t bit_index = 0;
	uint32_t prog_vaddr = 0;
	// 在父进程的用户空间中查找已有数据的页
	while(byte_index < byte_len) {
		if(vaddr_btmp[byte_index]) {
			bit_index = 0;
			while(bit_index < 8) {
				if(vaddr_btmp[byte_index] & (1 << bit_index)) {
					prog_vaddr = vaddr_start + (byte_index * 8 + bit_index) * PAGE_SIZE;
					// 将父进程用户空间中的数据通过内核空间中转,
					// 最终复制到子进程的用户空间
					// 1 将父进程用户空间的数据复制到内核缓冲区buf,
					// 目的是切换到子进程的页表后,还能访问到父进程的数据
					memcpy(buf, (void*) prog_vaddr, PAGE_SIZE);
					// 2 将页表切换到子进程的页表,防止数据复制到父进程的页表中
					pgdir_activate(child_thread);
					// 3 申请虚拟地址prog_vaddr
					prog_vaddr = (uint32_t) get_page_without_btmp(prog_vaddr);
					// 4 从内核缓冲区中将父进程数据复制到子进程的用户空间
					memcpy((void*) prog_vaddr, buf, PAGE_SIZE);
					// 5 恢复父进程页表
					pgdir_activate(parent_thread);
				}
				++bit_index;
			}
		}
		++byte_index;
	}
}

// 为子进程构建thread_stack和修改返回值
static void build_child_stack(struct task_struct *child_thread) {
	// 1 使子进程pid返回值为0
	// 获取子进程0级栈栈顶
	struct intr_stack *stack0 = (struct intr_stack*) \
		((uint32_t) child_thread + PAGE_SIZE  - sizeof(struct intr_stack));
	// 修改子进程的返回值为0
	stack0->eax = 0;
	// 2 为switch_to函数构建struct thread_stack,
	// 将其构建在紧邻intr_stack之下的空间
	// switch_to的返回地址
	uint32_t *ret_addr = (uint32_t*) stack0 - 1;
	// ebp在thread_stack中的地址就是当时的esp
	uint32_t *ebp = (uint32_t*) stack0 - 5;
	// switch_to的返回地址更新为intr_exit,直接从中断返回
	*ret_addr = (uint32_t) intr_exit;
	// 把构建的thread_stack的栈顶作为switch_to函数恢复数据时的栈顶
	child_thread->self_kstack = ebp;
}

// 更新inode的打开数
static void update_open_count(struct task_struct *thread) {
	int32_t local_fd = 3;
	int32_t global_fd = 0;
	while(local_fd < PROC_MAX_FILE_OPEN) {
		global_fd = thread->fd_table[local_fd];
		ASSERT(global_fd < MAX_FILE_OPEN);
		if(global_fd != -1) {
			++file_table[global_fd].fd_inode->open_count;
		}
		++local_fd;
	}
}

// 复制父进程本身所占资源给子进程
static int32_t copy_resource(struct task_struct *child_thread, \
	struct task_struct *parent_thread) {
	// 内核缓冲区,作为父子进程数据交换的中转
	void *buf = get_kernel_pages(1);
	if(buf == NULL) {
		return -1;
	}
	// 1 复制父进程的PCB,虚拟地址位图,内核栈到子进程
	copy_pcb(child_thread, parent_thread);
	// 2 为子进程创建页表,此页表只有内核空间
	child_thread->pgdir = create_pgdir();
	if(child_thread->pgdir == NULL) {
		return -1;
	}
	// 3 复制父进程进程体及用户栈给子进程
	copy_prog_body(child_thread, parent_thread, buf);
	// 4 构建子进程thread_stack和修改返回值pid
	build_child_stack(child_thread);
	// 5 更新文件inode的打开数
	update_open_count(child_thread);
	kfree(buf, 1);
	return 0;
}

// fork子进程,内核线程不可直接调用
pid_t sys_fork(void) {
	struct task_struct *parent_thread = current_thread();
	struct task_struct *child_thread = get_kernel_pages(1);
	// 为子进程创建PCB(task_struct结构)
	if(child_thread == NULL) {
		return -1;
	}
	ASSERT((INTR_OFF == get_intr_status()) \
		&& (parent_thread->pgdir != NULL));
	if(copy_resource(child_thread, parent_thread) == -1) {
		return -1;
	}
	// 添加到就绪线程队列和所有线程队列,子进程由调试器安排运行
	ASSERT(!list_find(&thread_ready_list, &child_thread->general_tag));
	list_append(&thread_ready_list, &child_thread->general_tag);
	ASSERT(!list_find(&thread_all_list, &child_thread->general_tag));
	list_append(&thread_all_list, &child_thread->all_list_tag);
	return child_thread->pid; // 父进程返回子进程的pid
}































































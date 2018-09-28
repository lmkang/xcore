#include "sync.h"
#include "interrupt.h"
#include "debug.h"

// 初始化信号量
void sema_init(struct semaphore *psema, uint8_t value) {
	psema->value = value;
	list_init(&psema->waiters);
}

// 初始化锁lock
void lock_init(struct lock *plock) {
	plock->holder = NULL;
	plock->repeat_num = 0;
	sema_init(&plock->semaphore, 1);
}

// 信号量down操作
void sema_down(struct semaphore *psema) {
	// 关中断来保证原子操作
	enum intr_status old_status = get_intr_status();
	disable_intr();
	while(psema->value == 0) { // 若value为0,表示已经被别人持有
		ASSERT(!has_ele(&psema->waiters, &current_thread()->general_tag));
		// 当前线程不应该已在信号量的waiters队列中
		if(has_ele(&psema->waiters, &current_thread()->general_tag)) {
			PANIC("sema_down: thread blocked has been in waiters_list\n");
		}
		// 若信号量的值为0,则当前线程把自己加入该锁的等待队列,然后阻塞自己
		list_append(&psema->waiters, &current_thread()->general_tag);
		thread_block(TASK_BLOCKED);
	}
	// 若value为1或被唤醒后,执行下面代码,即获得了锁
	--psema->value;
	ASSERT(psema->value == 0);
	set_intr_status(old_status);
}

// 信号量的up操作
void sema_up(struct semaphore *psema) {
	ASSERT(psema->value == 0);
	// 关中断,保证原子操作
	enum intr_status old_status = get_intr_status();
	disable_intr();
	if(!list_empty(&psema->waiters)) {
		struct task_struct *thread_blocked = ele2entry(struct task_struct, 
			general_tag, list_pop(&psema->waiters));
		thread_unblock(thread_blocked);
	}
	++psema->value;
	ASSERT(psema->value == 1);
	set_intr_status(old_status);
}

// 获得锁plock
void lock_acquire(struct lock *plock) {
	// 排除曾经自己已经持有锁但还未释放的情况
	if(plock->holder != current_thread()) {
		sema_down(&plock->semaphore);
		plock->holder = current_thread();
		ASSERT(plock->repeat_num == 0);
		plock->repeat_num = 1;
	} else {
		++plock->repeat_num;
	}
}

// 释放锁plock
void lock_release(struct lock *plock) {
	ASSERT(plock->holder == current_thread());
	if(plock->repeat_num > 1) {
		--plock->repeat_num;
		return;
	}
	ASSERT(plock->repeat_num == 1);
	plock->holder = NULL;
	plock->repeat_num = 0;
	sema_up(&plock->semaphore);
}
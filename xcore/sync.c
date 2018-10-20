#include "sync.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"

// 初始化信号量
void sema_init(struct semaphore *sema, uint8_t value) {
	sema->value = value; // 为信号量赋值
	list_init(&sema->waiters); // 初始化信号量的等待队列
}

// 初始化锁lock
void lock_init(struct lock *lock) {
	lock->holder = NULL;
	lock->repeat_count = 0;
	sema_init(&lock->semaphore, 1); // 信号量初值为1
}

// 信号量down操作
void sema_down(struct semaphore *sema) {
	// 关中断来保证原子操作
	enum intr_status old_status = get_intr_status();
	disable_intr();
	while(sema->value == 0) { // 若value为0,表示已经被别人持有
		struct task_struct *cur_thread = current_thread();
		// 当前线程不应该已在信号量的waiters队列中
		ASSERT(!list_find(&sema->waiters, &cur_thread->general_tag));
		if(list_find(&sema->waiters, &cur_thread->general_tag)) {
			PANIC("sema_down : thread blocked in waiters\n");
		}
		// 若信号量的值为0,则当前线程把自己加入该锁的等待队列,然后阻塞自己
		list_append(&sema->waiters, &cur_thread->general_tag);
		thread_block(TASK_BLOCKED);
	}
	// value为1或者被唤醒后,即获得了锁
	--sema->value;
	ASSERT(sema->value == 0);
	set_intr_status(old_status);
}

// 信号量的up操作
void sema_up(struct semaphore *sema) {
	// 关中断,保证原子操作
	enum intr_status old_status = get_intr_status();
	disable_intr();
	ASSERT(sema->value == 0);
	if(!list_empty(&sema->waiters)) {
		struct task_struct *thread_blocked = \
			ELE2ENTRY(struct task_struct, general_tag, list_pop(&sema->waiters));
		thread_unblock(thread_blocked);
	}
	++sema->value;
	ASSERT(sema->value == 1);
	set_intr_status(old_status);
}

// 获取锁lock
void lock_acquire(struct lock *lock) {
	// 排除已经持有锁但还未将其释放的情况
	if(lock->holder != current_thread()) {
		sema_down(&lock->semaphore);
		lock->holder = current_thread();
		ASSERT(lock->repeat_count == 0);
		lock->repeat_count = 1;
	} else {
		++lock->repeat_count;
	}
}

// 释放锁lock
void lock_release(struct lock *lock) {
	ASSERT(lock->holder == current_thread());
	if(lock->repeat_count > 1) {
		--lock->repeat_count;
		return;
	}
	ASSERT(lock->repeat_count == 1);
	lock->holder = NULL;
	lock->repeat_count = 0;
	sema_up(&lock->semaphore);
}
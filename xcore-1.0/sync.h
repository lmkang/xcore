#ifndef __SYNC_H
#define __SYNC_H

#include "thread.h"

// 信号量结构
struct semaphore {
	uint8_t value;
	struct list waiters;
};

// 锁结构
struct lock {
	struct task_struct *holder; // 锁的持有者
	struct semaphore semaphore; // 用二元信号量实现锁
	uint32_t repeat_count; // 锁的持有者重复申请锁的次数
};

void sema_init(struct semaphore *sema, uint8_t value);

void lock_init(struct lock *lock);

void sema_down(struct semaphore *sema);

void sema_up(struct semaphore *sema);

void lock_acquire(struct lock *lock);

void lock_release(struct lock *lock);

#endif
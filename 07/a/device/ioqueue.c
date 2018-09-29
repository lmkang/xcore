#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"

// 初始化ioqueue
void ioqueue_init(struct ioqueue *ioq) {
	// 初始化ioq的锁
	lock_init(&ioq->lock);
	// 生产者和消费者都置空
	ioq->producer = NULL;
	ioq->consumer = NULL;
	// 队列的首尾指针都指向缓冲区数组的第0个位置
	ioq->head = 0;
	ioq->tail = 0;
}

// 返回pos在缓冲区数组的下一个位置值
static int32_t next_pos(int32_t pos) {
	return (pos + 1) % BUF_SIZE;
}

// 判断队列是否已满
bool ioq_full(struct ioqueue *ioq) {
	ASSERT(get_intr_status() == INTR_OFF);
	return next_pos(ioq->head) == ioq->tail;
}

// 判断队列是否为空
bool ioq_empty(struct ioqueue *ioq) {
	ASSERT(get_intr_status() == INTR_OFF);
	return (ioq->head == ioq->tail);
}

// 使当前生产者或消费者在此缓冲区上等待
static void ioq_wait(struct task_struct **waiter) {
	ASSERT(*waiter == NULL && waiter != NULL);
	*waiter = current_thread();
	thread_block(TASK_BLOCKED);
}

// 唤醒waiter
static void ioq_wakeup(struct task_struct **waiter) {
	ASSERT(*waiter != NULL);
	thread_unblock(*waiter);
	*waiter = NULL;
}

// 消费者从ioq队列中获取一个字符
char ioq_getchar(struct ioqueue *ioq) {
	ASSERT(get_intr_status() == INTR_OFF);
	// 若缓冲区为空,把消费者ioq->consumer记为当前线程
	// 目的是将来生产者往缓冲区放字符后,生产者唤醒当前线程(消费者)
	while(ioq_empty(ioq)) {
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->consumer);
		lock_release(&ioq->lock);
	}
	char ch = ioq->buf[ioq->tail]; // 从缓冲区取出
	ioq->tail = next_pos(ioq->tail); // 移到下一个位置
	if(ioq->producer != NULL) {
		ioq_wakeup(&ioq->producer); // 唤醒生产者
	}
	return ch;
}

// 生产者往ioq队列中写入一个字符ch
void ioq_putchar(struct ioqueue *ioq, char ch) {
	ASSERT(get_intr_status() == INTR_OFF);
	// 若缓冲区已满,把生产者ioq->producer记为当前线程
	// 目的是将来消费者从缓冲区去字符后,唤醒当前线程(生产者)
	while(ioq_full(ioq)) {
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->producer);
		lock_release(&ioq->lock);
	}
	ioq->buf[ioq->head] = ch; // 写入一个字符
	ioq->head = next_pos(ioq->head); // 移到下一个位置
	if(ioq->consumer != NULL) {
		ioq_wakeup(&ioq->consumer); // 唤醒消费者
	}
}
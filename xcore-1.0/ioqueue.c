#include "ioqueue.h"
#include "interrupt.h"
#include "debug.h"

// 初始化io队列ioq
void ioqueue_init(struct ioqueue *ioq) {
	lock_init(&ioq->lock);
	ioq->producer = NULL;
	ioq->consumer = NULL;
	ioq->head = 0;
	ioq->tail = 0;
}

// 返回pos在缓冲区中的下一个位置值
static int32_t next_pos(int32_t pos) {
	return (pos + 1) % BUF_SIZE;
}

// 判断队列是否已满
bool ioq_full(struct ioqueue *ioq) {
	ASSERT(get_intr_status() == INTR_OFF);
	return next_pos(ioq->head) == ioq->tail;
}

// 判断队列是否已空
bool ioq_empty(struct ioqueue *ioq) {
	ASSERT(get_intr_status() == INTR_OFF);
	return ioq->head == ioq->tail;
}

// 使当前生产者或消费者在此缓冲区上等待
static void ioq_wait(struct task_struct **waiter) {
	ASSERT(waiter != NULL && *waiter == NULL);
	*waiter = current_thread();
	thread_block(TASK_BLOCKED);
}

// 唤醒waiter
static void wakeup(struct task_struct **waiter) {
	ASSERT(waiter != NULL && *waiter != NULL);
	thread_unblock(*waiter);
	*waiter = NULL;
}

// 消费者从ioq队列中获取一个字符
char ioq_getchar(struct ioqueue *ioq) {
	ASSERT(get_intr_status() == INTR_OFF);
	// 若缓冲区(队列)为空,把消费者ioq->consumer记为当前线程,
	// 目的是生产者唤醒消费者
	while(ioq_empty(ioq)) {
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->consumer);
		lock_release(&ioq->lock);
	}
	char ch = ioq->buf[ioq->tail];
	ioq->tail = next_pos(ioq->tail);
	if(ioq->producer != NULL) {
		wakeup(&ioq->producer);
	}
	return ch;
}

// 生产者往ioq队列中写入一个字符ch
void ioq_putchar(struct ioqueue *ioq, char ch) {
	ASSERT(get_intr_status() == INTR_OFF);
	// 若缓冲区(队列)已满,把生产者ioq->producer记为当前线程,
	// 目的是消费者唤醒生产者
	while(ioq_full(ioq)) {
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->producer);
		lock_release(&ioq->lock);
	}
	ioq->buf[ioq->head] = ch;
	ioq->head = next_pos(ioq->head);
	if(ioq->consumer != NULL) {
		wakeup(&ioq->consumer);
	}
}
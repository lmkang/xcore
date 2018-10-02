#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define BUF_SIZE 64

// 环形队列
// 解决生产者消费者问题
struct ioqueue {
	struct lock lock;
	struct task_struct *producer; // 生产者
	struct task_struct *consumer; // 消费者
	char buf[BUF_SIZE]; // 缓冲区数组
	int32_t head; // 队首,数据在队首写入
	int32_t tail; // 队尾,数据在队尾读取
};

void ioqueue_init(struct ioqueue *ioq);

bool ioq_full(struct ioqueue *ioq);

bool ioq_empty(struct ioqueue *ioq);

char ioq_getchar(struct ioqueue *ioq);

void ioq_putchar(struct ioqueue *ioq, char ch);

#endif
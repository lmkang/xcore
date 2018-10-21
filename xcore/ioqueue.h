#ifndef __IOQUEUE_H
#define __IOQUEUE_H

#include "sync.h"

#define BUF_SIZE 64

// 环形队列
struct ioqueue {
	struct lock lock;
	struct task_struct *producer;
	struct task_struct *consumer;
	char buf[BUF_SIZE];
	int32_t head;
	int32_t tail;
};

void ioqueue_init(struct ioqueue *ioq);

bool ioq_full(struct ioqueue *ioq);

bool ioq_empty(struct ioqueue *ioq);

char ioq_getchar(struct ioqueue *ioq);

void ioq_putchar(struct ioqueue *ioq, char ch);

#endif
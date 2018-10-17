#ifndef __LIST_H
#define __LIST_H

#include "types.h"

// 链表元素结构
struct list_ele {
	struct list_ele *prev; // 前驱节点
	struct list_ele *next; // 后继节点
};

// 链表结构
struct list {
	struct list_ele head; // 链表头
	struct list_ele tail; // 链表尾
};

#endif
#ifndef __LIST_H
#define __LIST_H

#include "types.h"

// 双向链表元素结构
struct list_ele {
	struct list_ele *prev; // 前驱节点
	struct list_ele *next; // 后继节点
};

// 双向链表结构
struct list {
	struct list_ele head; // 链表头
	struct list_ele tail; // 链表尾
};

// 自定义函数类型,用于在list_traversal中做回调函数
typedef bool (list_func)(struct list_ele *ele, int arg);

void list_init(struct list *list);

void list_insert_before(struct list_ele *before, struct list_ele *ele);

void list_push(struct list *list, struct list_ele *ele);

void list_append(struct list *list, struct list_ele *ele);

void list_remove(struct list_ele *ele);

struct list_ele *list_pop(struct list *list);

bool list_find(struct list *list, struct list_ele *ele);

bool list_empty(struct list *list);

struct list_ele *list_traversal(struct list *list, list_func func, int arg);

uint32_t list_len(struct list *list);

#endif
#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H
#include "global.h"
#include "stdint.h"

#define offset(struct_type, member_name) (int) (&((struct_type*) 0)->member_name)

#define ele2entry(struct_type, member_name, ele_ptr) \
(struct_type*) ((int) ele_ptr - offset(struct_type, member_name))

// 定义链表节点成员结构
// 节点中不需要数据成员,只要前驱和后继节点指针
struct list_ele {
	struct list_ele *prev; // 前驱节点
	struct list_ele *next; // 后继节点
};

// 链表结构,用来实现队列
struct list {
	struct list_ele head; // 队首,固定不变
	struct list_ele tail; // 队尾,固定不变
};

// 自定义函数类型list_func,用于在list_traversal中做回调函数
typedef bool list_func(struct list_ele *ele, int arg);

void list_init(struct list *plist);

void list_insert_before(struct list_ele *before, struct list_ele *ele);

void list_push(struct list *plist, struct list_ele *ele);

void list_iterate(struct list *plist);

void list_append(struct list *plist, struct list_ele *ele);

void list_remove(struct list_ele *ele);

struct list_ele * list_pop(struct list *plist);

bool list_empty(struct list *plist);

uint32_t list_len(struct list *plist);

struct list_ele * list_traversal(struct list *plist, list_func func, int arg);

bool has_ele(struct list *plist, struct list_ele *ele);

#endif
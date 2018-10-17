#include "types.h"
#include "list.h"
#include "interrupt.h"

// 初始化双向链表
void list_init(struct list *list) {
	list->head.prev = NULL;
	list->head.next = &list->tail;
	list->tail.prev = &list->head;
	list->tail.next = NULL;
}

// 把链表元素ele插入在元素before之前
void list_insert_before(struct list_ele *before, struct list_ele *ele) {
	enum intr_status old_status = get_intr_status();
	disable_intr();
	before->prev->next = ele;
	ele->prev = before->prev;
	ele->next = before;
	before->prev = ele;
	set_intr_status(old_status);
}

// 添加元素到链表队首
void list_push(struct list *list, struct list_ele *ele) {
	list_insert_before(list->head.next, ele);
}

// 追加元素到链表队尾
void list_append(struct list *list, struct list_ele *ele) {
	list_insert_before(&list->tail, ele);
}

// 从链表中移除元素ele
void list_remove(struct list_ele *ele) {
	enum intr_status old_status = get_intr_status();
	disable_intr();
	ele->prev->next = ele->next;
	ele->next->prev = ele->prev;
	set_intr_status(old_status);
}

// 将链表第一个元素弹出并返回
struct list_ele *list_pop(struct list *list) {
	struct list_ele *ele = list->head.next;
	list_remove(ele);
	return ele;
}

// 从链表中查找元素ele,成功返回true,失败返回false
bool list_find(struct list *list, struct list_ele *ele) {
	struct list_ele *p = list->head.next;
	while(p != &list->tail) {
		if(p == ele) {
			return true;
		}
		p = p->next;
	}
	return false;
}

// 判断链表是否为空,空返回true,否则返回false
bool list_empty(struct list *list) {
	return list->head.next == &list->tail ? true : false;
}

// 遍历链表,调用回调函数func(ele, arg),找到符合条件的元素
// func(ele, arg)返回true表示找到元素,停止遍历
// func(ele, arg)返回false表示未找到元素,继续遍历
struct list_ele *list_traversal(struct list *list, void *func, int arg) {
	if(list_empty(list)) {
		return NULL;
	}
	struct list_ele *ele = list->head.next;
	while(ele != &list->tail) {
		if(func(ele, arg)) {
			return ele;
		}
		ele = ele->next;
	}
	return NULL;
}

// 返回链表长度
uint32_t list_len(struct list *list) {
	struct list_ele *ele = list->head.next;
	uint32_t len = 0;
	while(ele != &list->tail) {
		++len;
		ele = ele->next;
	}
	return len;
}
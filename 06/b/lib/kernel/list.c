#include "list.h"
#include "interrupt.h"

// 初始化双向链表list
void list_init(struct list *plist) {
	plist->head.prev = NULL;
	plist->head.next = &plist->tail;
	plist->tail.prev = &plist->head;
	plist->tail.next = NULL;
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
void list_push(struct list *plist, struct list_ele *ele) {
	list_insert_before(plist->head.next, ele);
}

// 追加元素到链表队尾
void list_append(struct list *plist, struct list_ele *ele) {
	list_insert_before(&plist->tail, ele);
}

// 使元素ele脱离链表
void list_remove(struct list_ele *ele) {
	enum intr_status old_status = get_intr_status();
	disable_intr();
	ele->prev->next = ele->next;
	ele->next->prev = ele->prev;
	set_intr_status(old_status);
}

// 将链表第一个元素弹出并返回
struct list_ele * list_pop(struct list *plist) {
	struct list_ele *ele = plist->head.next;
	list_remove(ele);
	return ele;
}

// 判断链表中是否存在元素ele,成功返回true,失败返回false
bool has_ele(struct list *plist, struct list_ele *ele) {
	struct list_ele *tmp_ele = plist->head.next;
	while(tmp_ele != &plist->tail) {
		if(tmp_ele == ele) {
			return true;
		}
		tmp_ele = tmp_ele->next;
	}
	return false;
}

// 遍历链表所有元素,逐个判断是否有符合条件的元素
// 找到符合条件的元素返回元素指针,否则返回NULL
// 把链表plist中的每个元素ele和arg传给回调函数func
// arg用来判断ele是否符合条件
struct list_ele * list_traversal(struct list *plist, list_func func, int arg) {
	if(list_empty(plist)) {
		return NULL;
	}
	struct list_ele *ele = plist->head.next;
	while(ele != &plist->tail) {
		if(func(ele, arg)) {
			return ele;
		} else {
			ele = ele->next;
		}
	}
	return NULL;
}

// 返回链表的长度
uint32_t list_len(struct list *plist) {
	struct list_ele *ele = plist->head.next;
	uint32_t len = 0;
	while(ele != &plist->tail) {
		++len;
		ele = ele->next;
	}
	return len;
}

// 判断链表是否为空,为空返回true,否则返回false
bool list_empty(struct list *plist) {
	return plist->head.next == &plist->tail ? true : false;
}
#ifndef __BITMAP_H
#define __BITMAP_H

#include "types.h"

// 位图
struct bitmap {
	uint32_t byte_len; // 字节数组长度
	uint8_t *bits; // 字节数组
};

bool test_bitmap(struct bitmap *btmp, uint32_t index);

void set_bitmap(struct bitmap *btmp, uint32_t index, uint8_t value);

int alloc_bitmap(struct bitmap *btmp, uint32_t count);

void init_bitmap(struct bitmap *btmp);

#endif
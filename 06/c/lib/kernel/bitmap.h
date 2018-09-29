#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H
#include "stdint.h"
#include "global.h"

struct bitmap {
	uint32_t byte_len;
	uint8_t *bits;
};

void init_bitmap(struct bitmap *btmp);

bool test_bitmap(struct bitmap *btmp, uint32_t bit_idx);

int alloc_bitmap(struct bitmap *btmp, uint32_t count);

void set_bitmap(struct bitmap *btmp, uint32_t bit_idx, int8_t value);
#endif
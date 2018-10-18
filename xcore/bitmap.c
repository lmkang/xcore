#include "bitmap.h"
#include "string.h"

// 测试index位置的位是否为1,若为1返回true,否则返回false
bool test_bitmap(struct bitmap *btmp, uint32_t index) {
	uint32_t byte_idx = index / 8;
	uint32_t bit_idx = index % 8;
	return (byte_idx < btmp->byte_len) && ((btmp->bits[byte_idx] >> bit_idx) & 0x01);
}

// 设置index位置的位为value
void set_bitmap(struct bitmap *btmp, uint32_t index, uint8_t value) {
	uint32_t byte_idx = index / 8;
	uint32_t bit_idx = index % 8;
	if(value) {
		btmp->bits[byte_idx] |= (value << bit_idx);
	} else {
		btmp->bits[byte_idx] &= (value << bit_idx);
	}
}

// 分配连续count个位,成功返回位图的索引,失败返回-1
int alloc_bitmap(struct bitmap *btmp, uint32_t count) {
	// 1 先找到空闲字节
	uint32_t byte_idx = 0;
	while((byte_idx < btmp->byte_len) && (0xff == btmp->bits[byte_idx])) {
		++byte_idx;
	}
	if(byte_idx >= btmp->byte_len) {
		return -1;
	}
	// 2 再在空闲字节找到空闲位
	uint32_t bit_idx = 0;
	while((btmp->bits[byte_idx] >> bit_idx) & 0x01) {
		++bit_idx;
	}
	// 3 寻找空闲位数目
	uint32_t index = byte_idx * 8 + bit_idx; // 位图的索引
	uint32_t avail_count = btmp->byte_len * 8 - index;
	uint32_t find_count = 0;
	uint32_t next_idx = index;
	while(avail_count) {
		if(!test_bitmap(btmp, next_idx)) {
			++find_count;
		} else {
			find_count = 0;
		}
		if(find_count == count) {
			for(uint32_t i = 0; i < count; i++) {
				set_bitmap(btmp, (next_idx - count + 1) + i, 1);
			}
			return next_idx - count + 1;
		}
		++next_idx;
		--avail_count;
	}
	return -1;
}

// 初始化位图
void init_bitmap(struct bitmap *btmp) {
	memset(btmp->bits, 0, btmp->byte_len);
}
#include "stdint.h"
#include "global.h"
#include "string.h"
#include "debug.h"

#define BITMAP_MASK 1

// 初始化位图bitmap
void init_bitmap(struct bitmap *btmp) {
	memset(btmp->bits, 0, btmp->byte_len);
}

// 判断bit_idx位是否为1,若为1,返回true,否则返回false
bool test_bitmap(struct bitmap * btmp, uint32_t bit_idx) {
	uint32_t byte_idx = bit_idx / 8; // 向下取整,索引数组下标
	uint32_t bit_mod = bit_idx % 8; // 取余,索引数组内的位
	return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_mod));
}

// 在位图中申请连续count个位,成功返回起始位下标,失败返回-1
int alloc_bitmap(struct bitmap *btmp, uint32_t count) {
	uint32_t byte_idx = 0; // 记录空闲位所在的字节
	while((0xff == btmp->bits[byte_idx]) && (byte_idx < btmp->byte_len)) {
		++byte_idx;
	}
	ASSERT(byte_idx <= btmp->byte_len);
	if(byte_idx == btmp->byte_len) { // 在该内存池找不到可用空间
		return -1;
	}
	// 若在位图数组范围内的某字节内找到了空闲位
	// 在该字节内逐位比对,返回空闲位的索引
	uint32_t bit_idx = 0;
	while((uint8_t) (BITMAP_MASK << bit_idx) & btmp->bits[byte_idx]) {
		++bit_idx;
	}
	uint32_t bit_idx_start = byte_idx * 8 + bit_idx; // 空闲位在位图内的下标
	if(count == 1) {
		return bit_idx_start;
	}
	uint32_t bit_left = btmp->byte_len * 8 - bit_idx_start;
	uint32_t next_bit = bit_idx_start + 1;
	uint32_t find_count = 1; // 记录找到的空闲位的个数
	bit_idx_start = -1; // 将其置为-1,若找不到连续的位就直接返回
	while(bit_left-- > 0) {
		if(!test_bitmap(btmp, next_bit)) { // next_bit == 0
			++find_count;
		} else {
			find_count = 0;
		}
		if(find_count == count) { // 找到连续的count个空闲位
			bit_idx_start = next_bit - count + 1;
			break;
		}
		++next_bit;
	}
	return bit_idx_start;
}

// 设置位图btmp的bit_idx位
void set_bitmap(struct bitmap * btmp, uint32_t bit_idx, int8_t value) {
	ASSERT((value == 0) || (value == 1));
	uint32_t byte_idx = bit_idx / 8; // 向下取整,索引数组下标
	uint32_t bit_mod = bit_idx %8; // 取余,索引数组内的位
	if(value) {
		btmp->bits[byte_idx] |= (BITMAP_MASK << bit_mod);
	} else {
		btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_mod);
	}
}
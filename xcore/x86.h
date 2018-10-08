#ifndef __X86_H
#define __X86_H

#include "types.h"

// 向端口port写入一个字节
static inline void outb(uint16_t port, uint8_t data) {
	__asm__ __volatile__("outb %b0, %w1" : : "a"(data), "dN"(port));
}

// 从端口port读取一个字节返回
static inline uint8_t inb(uint16_t port) {
	uint8_t data;
	__asm__ __volatile__("inb %w1, %b0" : "=a"(data) : "dN"(port));
	return data;
}

#endif
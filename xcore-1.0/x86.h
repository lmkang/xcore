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

// 将addr处起始的count个字写入端口port
static inline void outsw(uint16_t port, const void *addr, uint32_t count) {
	__asm__ __volatile__("cld; rep outsw" : "+S"(addr), "+c"(count) : "d"(port));
}

// 将端口port读入的count个字写入addr
static inline void insw(uint16_t port, void *addr, uint32_t count) {
	__asm__ __volatile__("cld; rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

#endif
#include "types.h"
#include "debug.h"

// 将dst起始的size个字节置为value
void memset(void *dst, uint8_t value, uint32_t size) {
	ASSERT(dst != NULL);
	uint8_t *_dst = (uint8_t*) dst;
	while(size-- > 0) {
		*_dst++ = value;
	}
}

// 将src起始的size个字节复制到dst
void memcpy(void *dst, const void *src, uint32_t size) {
	ASSERT(dst != NULL && src != NULL);
	uint8_t *_dst = (uint8_t*) dst;
	const uint8_t *_src = (const uint8_t*) src;
	while(size-- > 0) {
		*_dst++ = *_src++;
	}
}

// 连续比较以地址s1和地址s2开头的size个字节
// s1==s2返回0,s1>s2返回+1,s1<s2返回-1
int memcmp(const void *s1, const void *s2, uint32_t size) {
	ASSERT(s1 != NULL || s2 != NULL);
	const char *_s1 = (const char*) s1;
	const char *_s2 = (const char*) s2;
	while(size-- > 0) {
		if(*_s1 != *_s2) {
			return *_s1 > *_s2 ? 1 : -1;
		}
		++_s1;
		++_s2;
	}
	return 0;
}

// 将字符串从src复制到dst
char * strcpy(char *dst, const char *src) {
	ASSERT(dst != NULL && src != NULL);
	char *_dst = dst;
	while((*dst++ = *src++));
	return _dst;
}

// 返回字符串长度
uint32_t strlen(const char *str) {
	ASSERT(str != NULL);
	const char *p = str;
	while(*p++);
	return (p - str - 1);
}

// 比较两个字符串
// s1==s2返回0,s1>s2返回1,s1<s2返回-1
int strcmp(const char *s1, const char *s2) {
	ASSERT(s1 != NULL && s2 != NULL);
	while(*s1 != 0 && *s1 == *s2) {
		++s1;
		++s2;
	}
	return *s1 < *s2 ? -1 : *s1 > *s2;
}

// 从左到右查找字符串str中首次出现字符ch的地址
char * strchr(const char *str, const uint8_t ch) {
	ASSERT(str != NULL);
	while(*str != 0) {
		if(*str == ch) {
			return (char*) str;
		}
		++str;
	}
	return NULL;
}

// 从后往前查找字符串str首次出现字符ch的地址
char * strrchr(char *str, const uint8_t ch) {
	ASSERT(str != NULL);
	const char *last_ch = NULL;
	while(*str != 0) {
		if(*str == ch) {
			last_ch = str;
		}
		++str;
	}
	return (char*) last_ch;
}

// 将字符串src拼接到dst后,返回拼接后的字符串地址
char * strcat(char *dst, const char *src) {
	ASSERT(dst != NULL && src != NULL);
	char *str = dst;
	while(*str++);
	--str;
	while((*str++ = *src++));
	return dst;
}

// 在字符串str中查找字符ch出现的次数
uint32_t strchrs(const char *str, uint8_t ch) {
	ASSERT(str != NULL);
	uint32_t count = 0;
	const char *p = str;
	while(*p != 0) {
		if(*p == ch) {
			++count;
		}
		++p;
	}
	return count;
}
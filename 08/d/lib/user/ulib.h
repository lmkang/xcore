#ifndef __LIB_USER_ULIB_H
#define __LIB_USER_ULIB_H
#include "stdint.h"

typedef char* va_list;

uint32_t getpid();

uint32_t write(char *str);

void * malloc(uint32_t size);

void free(void *ptr);

uint32_t vsprintf(char *str, const char *format, va_list ap);

uint32_t sprintf(char *buf, const char *format, ...);

uint32_t printf(const char *format, ...);

#endif
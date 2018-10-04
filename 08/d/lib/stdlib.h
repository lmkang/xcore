#ifndef __LIB_STDLIB_H
#define __LIB_STDLIB_H
#include "stdint.h"

void * malloc(uint32_t size);

void free(void *ptr);

#endif
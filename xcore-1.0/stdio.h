#ifndef __STDIO_H
#define __STDIO_H

#include "types.h"

typedef char* va_list;

#define va_start(ap, v) ap = (va_list) &v
#define va_arg(ap, t) *((t*) (ap += 4))
#define va_end(ap) ap = NULL

uint32_t vsprintf(char *str, const char *format, va_list ap);

uint32_t sprintf(char *buf, const char *format, ...);

uint32_t printf(const char *format, ...);

#endif
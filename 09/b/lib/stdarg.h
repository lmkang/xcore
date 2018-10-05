#ifndef __LIB_STDARG_H
#define __LIB_STDARG_H

typedef char* va_list;

#define va_start(ap, v) ap = (va_list) &v
#define va_arg(ap, t) *((t*) (ap += 4))
#define va_end(ap) ap = NULL

#endif
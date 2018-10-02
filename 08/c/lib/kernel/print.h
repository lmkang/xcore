#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H
#include "../stdint.h"
void put_char(uint8_t ch);
void put_str(char *str);
void put_int(uint32_t i);
void set_cursor(uint32_t cursor_pos);
#endif
#ifndef __PRINT_H
#define __PRINT_H

#include "types.h"

void set_cursor(uint16_t cursor_pos);

void put_char(uint8_t ch);

void put_str(char *str);

void put_hex(uint32_t i);

#endif
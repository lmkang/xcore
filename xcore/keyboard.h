#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include "ioqueue.h"

extern struct ioqueue kbd_buf;

void keyboard_init(void);

#endif
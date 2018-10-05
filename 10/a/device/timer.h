#ifndef __DEVICE_TIME_H
#define __DEVICE_TIME_H
#include "stdint.h"

void init_timer(void);

void mill_sleep(uint32_t millseconds);

#endif
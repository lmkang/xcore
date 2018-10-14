#ifndef __INTERRUPT_H
#define __INTERRUPT_H

#include "types.h"

// 定义中断的两种状态
// INTR_OFF值为0,表示关中断
// INTR_ON值为1,表示开中断
enum intr_status {
	INTR_OFF,
	INTR_ON
};

void init_idt(void);

void register_intr_handler(uint8_t vec_no, void *func);

enum intr_status get_intr_status(void);

void enable_intr(void);

void disable_intr(void);

void set_intr_status(enum intr_status status);

#endif
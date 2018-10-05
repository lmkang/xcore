#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"

typedef void* intr_handler;
void init_idt(void);

// 定义中断的两种状态
// INTR_OFF值为0,表示关中断
// INTR_ON值为1,表示开中断
enum intr_status {
	INTR_OFF,
	INTR_ON
};

enum intr_status get_intr_status(void);
void set_intr_status(enum intr_status);
void enable_intr(void);
void disable_intr(void);
void register_handler(uint8_t vec_no, intr_handler func);
#endif
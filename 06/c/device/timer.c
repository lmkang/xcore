#include "stdint.h"
#include "x86.h"
#include "print.h"
#include "thread.h"
#include "debug.h"

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0
#define COUNTER_MODE 2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PORT 0x43

uint32_t ticks; // ticks是内核自中断开启以来总共的嘀嗒数

// 设置timer
static void set_timer(uint8_t counter_port, uint8_t counter_no, 
	uint8_t rwl, uint8_t counter_mode, uint16_t counter_value) {
	outb(PIT_CONTROL_PORT, (uint8_t) (counter_no << 6 | rwl << 4 | counter_mode << 1));
	outb(counter_port, (uint8_t) counter_value);
	outb(counter_port, (uint8_t) counter_value >> 8);
}

// 时钟的中断处理函数
static void intr_timer_handler(void) {
	struct task_struct *cur_thread = current_thread();
	ASSERT(cur_thread->stack_magic == 0x19940625); // 检查栈是否溢出
	++cur_thread->elapsed_ticks; // 记录此线程占用的CPU时间
	++ticks; // 内核态和用户态总共的嘀嗒数
	if(cur_thread->ticks == 0) { // 若进程时间片用完,调度新进程上CPU
		schedule();
	} else { // 将当前进程的时间片-1
		--cur_thread->ticks;
	}
}

// 初始化timer
void init_timer() {
	put_str("init_timer start\n");
	set_timer(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
	register_handler(0x20, intr_timer_handler);
	put_str("init_timer done\n");
}
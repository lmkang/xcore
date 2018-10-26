#include "x86.h"
#include "print.h"
#include "thread.h"
#include "interrupt.h"
#include "debug.h"
#include "global.h"

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE (INPUT_FREQUENCY / IRQ0_FREQUENCY)
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0
#define COUNTER_MODE 2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PORT 0x43

#define MILLSECONDS_PER_INTR (1000 / IRQ0_FREQUENCY)

uint32_t ticks; // ticks是内核自中断开启以来总共的嘀嗒数

// 设置计数器
static void set_timer(uint8_t counter_port, uint8_t counter_no, \
	uint8_t rwl, uint8_t counter_mode, uint16_t counter_value) {
	// 往控制字寄存器端口0x43中写入控制字
	outb(PIT_CONTROL_PORT, (uint8_t) (counter_no << 6 | rwl << 4 | counter_mode << 1));
	// 先写入counter_value的低8位
	outb(counter_port, (uint8_t) counter_value);
	// 再写入counter_value的高8位
	outb(counter_port, (uint8_t) counter_value >> 8);
}

// 时钟的中断处理函数
static void intr_timer_handler(void) {
	struct task_struct *cur_thread = current_thread();
	ASSERT(cur_thread->stack_magic == 0x19940625); // 检查栈是否溢出
	++cur_thread->elapsed_ticks; // 记录此线程占用的CPU时间
	++ticks; // 中断开启以来,内核态和用户态总共的嘀嗒数
	if(cur_thread->ticks == 0) {
		schedule();
	} else {
		--cur_thread->ticks;
	}
}

// 以ticks为单位的sleep,任何时间形式的sleep会转换成ticks形式
static void ticks2sleep(uint32_t sleep_ticks) {
	uint32_t start_ticks = ticks;
	// 若间隔的ticks数不够就让出CPU
	while(ticks - start_ticks < sleep_ticks) {
		thread_yield();
	}
}

// 以毫秒为单位的sleep,1s = 1000ms
void sleep(uint32_t millseconds) {
	uint32_t sleep_ticks = DIV_ROUND_UP(millseconds, MILLSECONDS_PER_INTR);
	ASSERT(sleep_ticks > 0);
	ticks2sleep(sleep_ticks);
}

// 初始化PIT8253
void init_timer() {
	// 设置8253的定时周期,即发中断的周期
	set_timer(COUNTER0_PORT, COUNTER0_NO, \
		READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
	register_intr_handler(0x20, intr_timer_handler);
	printk("init_timer done\n");
}
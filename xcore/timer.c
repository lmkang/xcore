#include "x86.h"
#include "print.h"

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE (INPUT_FREQUENCY / IRQ0_FREQUENCY)
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0
#define COUNTER_MODE 2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PORT 0x43

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

// 初始化PIT8253
void init_timer() {
	// 设置8253的定时周期,即发中断的周期
	set_timer(COUNTER0_PORT, COUNTER0_NO, \
		READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
	printk("init_timer done\n");
}
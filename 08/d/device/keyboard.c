#include "print.h"
#include "interrupt.h"
#include "global.h"
#include "x86.h"
#include "ioqueue.h"

#define KEYBOARD_BUF_PORT 0x60 // 键盘buffer寄存器端口号

// 用转义字符定义部分控制字符
#define esc '\033' // 八进制表示,十六进制是'\x1b'
#define backspace '\b'
#define tab '\t'
#define enter '\r'
#define delete '\177' // 八进制表示,十六进制是'\x7f'

// 不可见字符一律定义为0
#define ctrl_left_char 0
#define ctrl_right_char 0
#define shift_left_char 0
#define shift_right_char 0
#define alt_left_char 0
#define alt_right_char 0
#define capslock_char 0

// 定义控制字符的通码和断码
#define shift_left_make 0x2a
#define shift_right_make 0x36
#define alt_left_make 0x38
#define alt_right_make 0xe038
#define alt_right_break 0xe0b8
#define ctrl_left_make 0x1d
#define ctrl_right_make 0xe01d
#define ctrl_right_break 0xe09d
#define capslock_make 0x3a

// 定义变量记录相应键的按下状态
// ext_scancode用于记录makecode是否以0xe0开头
static bool ctrl_status, shift_status, alt_status, capslock_status, ext_scancode;

struct ioqueue keyboard_buf; // 键盘缓冲区

// 以通码makecode为索引的二维数组
static char keymap[][2] = {
	// 扫描码未与shift组合
	{0, 0},
	{esc, esc},
	{'1', '!'},
	{'2', '@'},
	{'3', '#'},
	{'4', '$'},	
	{'5', '%'},
	{'6', '^'},
	{'7', '&'},
	{'8', '*'},
	{'9', '('},
	{'0', ')'},
	{'-', '_'},
	{'=', '+'},
	{backspace, backspace},
	{tab, tab},
	{'q', 'Q'},
	{'w', 'W'},
	{'e', 'E'},
	{'r', 'R'},
	{'t', 'T'},
	{'y', 'Y'},
	{'u', 'U'},
	{'i', 'I'},
	{'o', 'O'},
	{'p', 'P'},
	{'[', '{'},
	{']', '}'},
	{enter, enter},
	{ctrl_left_char, ctrl_left_char},
	{'a', 'A'},
	{'s', 'S'},
	{'d', 'D'},
	{'f', 'F'},
	{'g', 'G'},
	{'h', 'H'},
	{'j', 'J'},
	{'k', 'K'},
	{'l', 'L'},
	{';', ':'},
	{'\'', '"'},
	{'`', '~'},
	{shift_left_char, shift_left_char},
	{'\\', '|'},
	{'z', 'Z'},
	{'x', 'X'},
	{'c', 'C'},
	{'v', 'V'},
	{'b', 'B'},
	{'n', 'N'},
	{'m', 'M'},
	{',', '<'},
	{'.', '>'},
	{'/', '?'},
	{shift_right_char, shift_right_char},
	{'*', '*'},
	{alt_left_char, alt_left_char},
	{' ', ' '},
	{capslock_char, capslock_char}
	// 其他按键暂不处理
};

// 键盘中断处理程序
static void intr_keyboard_handler(void) {
	// 中断发生前的上一次中断,以下任意三个键是否有按下
	bool ctrl_down_last = ctrl_status;
	bool shift_down_last = shift_status;
	bool capslock_down_last = capslock_status;
	bool breakcode;
	uint16_t scancode = inb(KEYBOARD_BUF_PORT);
	// 若scancode是e0开头,表示此键的按下将产生多个扫描码
	// 所以马上结束此次中断处理函数,等待下一个扫描码进来
	if(scancode == 0xe0) {
		ext_scancode = true; // 打开e0标记
		return;
	}
	// 如果上次是以0xe0开头的,将扫描码合并
	if(ext_scancode) {
		scancode = 0xe000 | scancode;
		ext_scancode = false; // 关闭e0标记
	}
	// 获取breakcode
	breakcode = ((scancode & 0x0080) != 0);
	if(breakcode) { // 若是断码
		// 由于ctrl_right和alt_right的通码和断码都是两字节
		// 所以可用下面的方法获取makecode,多字节的扫描码暂不处理
		uint16_t makecode = (scancode &= 0xff7f);
		// 任意三个键弹起,将其状态置为false
		if(makecode == ctrl_left_make || makecode == ctrl_right_make) {
			ctrl_status = false;
		} else if(makecode == shift_left_make || makecode == shift_right_make) {
			shift_status = false;
		} else if(makecode == alt_left_make || makecode == alt_right_make) {
			alt_status = false;
		} // 由于capslock不是弹起后关闭,所以需要单独处理
		return; // 直接结束此次中断处理
	} else if((scancode > 0x00 && scancode < 0x3b)
		|| (scancode == alt_right_make)
		|| (scancode == ctrl_right_make)) { // 若为通码
		// 判断是否与shift组合,用来在一维数组中索引对应的字符
		bool shift = false;
		// 0x0e:数字'0'~'9',字符'-',字符'='
		// 0x29:字符'`'
		// 0x1a:字符'['
		// 0x1b:字符']'
		// 0x2b:字符'\\'
		// 0x27:字符';'
		// 0x28:字符'\''
		// 0x33:字符','
		// 0x34:字符'.'
		// 0x35:字符'/'
		if((scancode < 0x0e) || (scancode == 0x29)
			|| (scancode == 0x1a) || (scancode == 0x1b)
			|| (scancode == 0x2b) || (scancode == 0x27)
			|| (scancode == 0x28) || (scancode == 0x33)
			|| (scancode == 0x34) || (scancode == 0x35)) {
			// 如果同时按下了shift键
			if(shift_down_last) {
				shift = true;
			}
		} else { // 默认为字母键
			if(shift_down_last && capslock_down_last) { // 如果shift和capslock同时按下
				shift = false;
			} else if(shift_down_last || capslock_down_last){ // shift或capslock被按下
				shift = true;
			} else {
				shift = false;
			}
		}
		// 将扫描码的高字节置为0,主要针对高字节是e0的扫描码
		uint8_t index = (scancode &= 0x00ff);
		// 在数组中找到对应的字符
		char cur_char = keymap[index][shift];
		// 只处理ASCII码不为0的键
		if(cur_char) {
			if(!ioq_full(&keyboard_buf)) {
				put_char(cur_char); // 临时
				ioq_putchar(&keyboard_buf, cur_char);
			}
			return;
		}
		// 记录本次是否按下了下面几类控制键之一,供下次判断组合键
		if((scancode == ctrl_left_make) || (scancode == ctrl_right_make)) {
			ctrl_status = true;
		} else if((scancode == shift_left_make) || (shift_right_make)) {
			shift_status = true;
		} else if((scancode == alt_left_make) || (scancode == alt_right_make)) {
			alt_status = true;
		} else if(scancode == capslock_make) {
			// capslock取反
			capslock_status = !capslock_status;
		}
	} else {
		put_str("unknown key\n");
	}
}

// 键盘初始化
void keyboard_init(void) {
	put_str("keyboard_init start\n");
	ioqueue_init(&keyboard_buf);
	register_handler(0x21, intr_keyboard_handler);
	put_str("keyboard_init done\n");
}
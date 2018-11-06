#include "x86.h"
#include "interrupt.h"
#include "print.h"
#include "ioqueue.h"

// 键盘buffer寄存器端口号为0x60
#define KBD_BUF_PORT 0x60

// 用转义字符定义部分控制字符
#define esc '\033'
#define backspace '\b'
#define tab '\t'
#define enter '\r'
#define delete '\177'

// 以上不可见字符一律定义为0
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

// 定义以下变量记录相应键是否按下的状态
static bool ctrl_status, shift_status, alt_status, capslock_status, ext_scancode;

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

// 定义键盘缓冲区
struct ioqueue kbd_buf;

// 键盘中断处理程序
static void intr_keyboard_handler(void) {
	// 本次中断发生前的上一次中断,以下任意三个键是否被按下
	bool ctrl_down_last = ctrl_status;
	bool shift_down_last = shift_status;
	bool capslock_last = capslock_status;
	
	bool break_code;
	uint16_t scancode = inb(KBD_BUF_PORT);
	// 若扫描码scancode是e0开头的,表示此键的按下将产生多个扫描码,
	// 所以马上结束本次中断处理函数,等待下一个扫描码进来
	if(scancode == 0xe0) {
		ext_scancode = true; // 打开e0标记
		return;
	}
	// 如果上次是以0xe0开头的,将扫描码合并
	if(ext_scancode) {
		scancode = (0xe000 | scancode);
		ext_scancode = false; // 关闭e0标记
	}
	break_code = ((scancode & 0x0080) != 0); // 获取break_code
	if(break_code) { // 若是断码
		// 由于ctrl_right和alt_right的make_code和break_code都是两字节,
		// 所以可用下面的方法取make_code,多字节的扫描码暂不处理
		uint16_t make_code = (scancode &= 0xff7f); // 得到其make_code
		// 若是任意以下三个键弹起了,将状态置为false
		if((make_code == ctrl_left_make) || (make_code == ctrl_right_make)) {
			ctrl_status = false;
		} else if((make_code == shift_left_make) || (make_code == shift_right_make)) {
			shift_status = false;
		} else if((make_code == alt_left_make) || (make_code == alt_right_make)) {
			alt_status = false;
		}
		// 由于capslock不是弹起后关闭,所以需要单独处理
		return;
	} else if(((scancode > 0x00) && (scancode < 0x3b))
		|| (scancode == alt_right_make)
		|| (scancode == ctrl_right_make)) { // 若为通码,只处理数组中定义的键以及alt_right和ctrl键
		// 判断是否与shift组合,用来在一维数组中索引对应的字符
		bool shift = false;
		if((scancode < 0x0e) || (scancode == 0x29)
			|| (scancode == 0x1a) || (scancode == 0x1b)
			|| (scancode == 0x2b) || (scancode == 0x27)
			|| (scancode == 0x28) || (scancode == 0x33)
			|| (scancode == 0x34) || (scancode == 0x35)) {
			if(shift_down_last) { // 如果同时按下了shift键
				shift = true;
			}
		} else { // 默认为字母键
			if(shift_down_last && capslock_last) { // shift和capslock同时按下
				shift = true;
			} else if(shift_down_last || capslock_last) { // shift或capslock被按下
				shift = true;
			} else {
				shift = false;
			}
		}
		// 将扫描码的高字节置0,主要针对高字节是e0的扫描码
		uint8_t index = (scancode &= 0x00ff);
		char cur_char = keymap[index][shift]; // 在数组中找到对应的字符
		// 只处理ASCII码不为0的键
		if(cur_char) {
			// 快捷键ctrl+l和ctrl+u的处理
			// 下面是把ctrl+l和ctrl+u这两种组合键产生的字符置为:
			// cur_char的asc码-字符a的asc码, 此差值比较小,
			// 属于ascii码表中不可见的字符部分.故不会产生可见字符
			// 我们在shell中将ascii值为l-a和u-a的分别处理为清屏和删除输入的快捷键
			if ((ctrl_down_last && cur_char == 'l') || (ctrl_down_last && cur_char == 'u')) {
				cur_char -= 'a';
			}
			// 若kbd_buf中未满并且待加入的cur_char不为0,则将其加入kbd_buf
			if(!ioq_full(&kbd_buf)) {
				ioq_putchar(&kbd_buf, cur_char);
			}
			return;
		}
		// 记录本次是否按下了下面几类控制键之一,供下次键入时判断组合键
		if((scancode == ctrl_left_make) || (scancode == ctrl_right_make)) {
			ctrl_status = true;
		} else if((scancode == shift_left_make) || (scancode == shift_right_make)) {
			shift_status = true;
		} else if((scancode == alt_left_make) || (scancode == alt_right_make)) {
			alt_status = true;
		} else if(scancode == capslock_make) {
			// capslock按键,状态取反
			capslock_status = !capslock_status;
		}
	} else {
		put_str("unknown key\n");
	}
}

// 键盘初始化
void keyboard_init(void) {
	ioqueue_init(&kbd_buf);
	register_intr_handler(0x21, intr_keyboard_handler);
	put_str("keyboard_init done\n");
}
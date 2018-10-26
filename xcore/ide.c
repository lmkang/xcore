#include "ide.h"
#include "global.h"
#include "debug.h"
#include "stdio.h"

// 定义硬盘各寄存器的端口号
#define reg_data(channel) (channel->port_base + 0)
#define reg_error(channel) (channel->port_base + 1)
#define reg_sector_count(channel) (channel->port_base + 2)
#define reg_lba_low(channel) (channel->port_base + 3)
#define reg_lba_mid(channel) (channel->port_base + 4)
#define reg_lba_high(channel) (channel->port_base + 5)
#define reg_dev(channel) (channel->port_base + 6)
#define reg_status(channel) (channel->port_base + 7)
#define reg_cmd(channel) (reg_status(channel))
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_ctrl(channel) (reg_alt_status(channel))

// reg_alt_status寄存器的一些关键位
#define BIT_ALT_STAT_BSY 0x80 // 硬盘忙
#define BIT_ALT_STAT_DRDY 0x40 // 驱动器准备好了
#define BIT_ALT_STAT_DRQ 0x8 // 数据传输准备好了

// device寄存器的一些关键位
#define BIT_DEV_MBS 0xa0 // 第7位和第5位固定为1
#define BIT_DEV_LBA 0x40
#define BIT_DEV_DEV 0x10

// 一些硬盘操作的指令
#define CMD_IDENTIFY 0xec // identify指令
#define CMD_READ_SECTOR 0x20 // 读扇区指令
#define CMD_WRITE_SECTOR 0x30 // 写扇区指令

// 定义可读写的最大扇区数,调试用的
#define MAX_LBA (100 * 1024 * 1024 / 512 - 1) // 只支持80MB硬盘

// 按硬盘数计算的通道数
uint8_t channel_count;

// 有两个ide通道
struct ide_channel channels[2];

// 硬盘数据结构初始化
void ide_init(void) {
	uint8_t disk_count = *((uint8_t*) P2V(0x475));
	ASSERT(disk_count > 0);
	// 一个ide通道有两个硬盘,根据硬盘数量算出ide通道数
	channel_count = DIV_ROUND_UP(disk_count, 2);
	struct ide_channel *channel;
	uint8_t channel_no = 0;
	// 处理每个通道上的硬盘
	while(channel_no < channel_count) {
		channel = &channels[channel_no];
		sprintf(channel->name, "ide%d", channel_no);
		// 为每个ide通道初始化端口基址和中断向量
		switch(channel_no) {
			case 0:
				channel->port_base = 0x1f0;
				channel->irq_no = 0x20 + 14;
				break;
			case 1:
				channel->port_base = 0x170;
				channel->irq_no = 0x20 + 15;
				break;
		}
		// 未向硬盘写入指令时不期待硬盘的中断
		channel->expect_intr = false;
		lock_init(&channel->lock);
		// 初始化为0,目的是向硬盘控制器请求数据后,
		// 硬盘驱动sema_down会阻塞线程,
		// 直到硬盘完成后通过发中断,
		// 由中断处理程序将此信号量sema_up唤醒线程
		sema_init(&channel->disk_done, 0);
		++channel_no;
	}
	printk("ide_init done\n");
}



















































































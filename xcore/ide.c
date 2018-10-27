#include "ide.h"
#include "global.h"
#include "debug.h"
#include "stdio.h"
#include "x86.h"
#include "timer.h"

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

// 选择读写的硬盘
static void select_disk(struct disk *disk) {
	uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
	if(disk->dev_no == 1) { // 从盘
		reg_device |= BIT_DEV_DEV;
	}
	outb(reg_dev(disk->channel), reg_device);
}

// 向硬盘控制器写入起始扇区地址及要读写的扇区数
static void select_sector(struct disk *disk, uint32_t lba_start, uint32_t sector_count) {
	ASSERT(lba_start <= MAX_LBA);
	struct ide_channel *channel = disk->channel;
	// 写入要读写的扇区数,sector_count为0表示写入256个扇区
	outb(reg_sector_count(channel), sector_count);
	//写入lba地址,即扇区号
	outb(reg_lba_low(channel), lba_start);
	outb(reg_lba_mid(channel), lba_start >> 8);
	outb(reg_lba_high(channel), lba_start >> 16);
	// lba地址的24-27位要存储在device寄存器的0-3位
	// 需要把device寄存器重写一次
	outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | \
		(disk->dev_no == 1 ? BIT_DEV_DEV : 0) | lba_start >> 24);
}

// 向通道channel发命令cmd
static void out_cmd(struct ide_channel *channel, uint8_t cmd) {
	// 只要向硬盘发出了命令便置为true
	// 硬盘中断处理程序需要根据它来判断
	channel->expect_intr = true;
	outb(reg_cmd(channel), cmd);
}

// 硬盘读入sector_count个扇区的数据到buf
static void read_sector(struct disk *disk, void *buf, uint8_t sector_count) {
	uint32_t byte_size;
	if(sector_count == 0) {
		byte_size = 256 * 512;
	} else {
		byte_size = sector_count * 512;
	}
	insw(reg_data(disk->channel), buf, byte_size / 2);
}

// 将buf中sector_count个扇区的数据写入硬盘
static void write_sector(struct disk *disk, void *buf, uint8_t sector_count) {
	uint32_t byte_size;
	if(sector_count == 0) {
		byte_size = 256 * 512;
	} else {
		byte_size = sector_count * 512;
	}
	outsw(reg_data(disk->channel), buf, byte_size / 2);
}

// 等待30秒
static bool busy_wait(struct disk *disk) {
	struct ide_channel *channel = disk->channel;
	uint16_t millseconds = 30 * 1000;
	
}

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



















































































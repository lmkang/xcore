#include "ide.h"
#include "console.h"
#include "ulib.h"
#include "x86.h"
#include "timer.h"
#include "ulib.h"
#include "debug.h"
#include "interrupt.h"
#include "sync.h"

// 定义硬盘各寄存器的端口号
#define reg_data(channel) (channel->base_port + 0)
#define reg_error(channel) (channel->base_port + 1)
#define reg_sector_count(channel) (channel->base_port + 2)
#define reg_lba_low(channel) (channel->base_port + 3)
#define reg_lba_mid(channel) (channel->base_port + 4)
#define reg_lba_high(channel) (channel->base_port + 5)
#define reg_dev(channel) (channel->base_port + 6)
#define reg_status(channel) (channel->base_port + 7)
#define reg_cmd(channel) (reg_status(channel))
#define reg_alt_status(channel) (channel->base_port + 0x206)
#define reg_ctrl(channel) (reg_alt_status(channel))

// reg_status寄存器的一些关键位
#define BIT_STAT_BSY 0x80 // 硬盘忙
#define BIT_STAT_DRDY 0x40 // 驱动器准备好
#define BIT_STAT_DRQ 0x8 // 数据传输准备好

// device寄存器的一些关键位
#define BIT_DEV_MBS 0xa0 // 第7位和第5位固定为1
#define BIT_DEV_LBA 0x40
#define BIT_DEV_DEV 0x10

// 一些硬盘操作的指令
#define CMD_IDENTIFY 0xec // identify指令
#define CMD_READ_SECTOR 0x20 // 读扇区指令
#define CMD_WRITE_SECTOR 0x30 // 写扇区指令

// 定义可读写的最大扇区数,调试用
#define MAX_LBA ((80 * 1024 * 1024 / 512) - 1) // 只支持80MB硬盘

uint8_t channel_count; // 按硬盘数计算的通道数
struct ide_channel channels[2]; // 有两个ide通道

// 选择读写的硬盘
static void select_disk(struct disk *disk) {
	uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
	if(disk->dev_no == 1) { // 如果是从盘,DEV=1
		reg_device |= BIT_DEV_DEV;
	}
	outb(reg_dev(disk->channel), reg_device);
}

// 向硬盘控制器写入起始扇区地址及要读写的扇区数
static void select_sector(struct disk * disk, uint32_t lba, uint8_t sector_count) {
	ASSERT(lba <= MAX_LBA);
	struct ide_channel *channel = disk->channel;
	// 写入要读写的扇区数,sector_count为0表示写入256个扇区
	outb(reg_sector_count(channel), sector_count);
	// 写入LBA地址,即扇区号
	outb(reg_lba_low(channel), lba);
	outb(reg_lba_mid(channel), lba >> 8);
	outb(reg_lba_high(channel), lba >> 16);
	// 因为LBA地址的第24~27位要存储到device寄存器的0~3位,
	// 无法单独写入这4位,所以在此处把device寄存器再重新写入一次
	outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (disk->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

// 向通道channel发命令cmd
static void cmd_out(struct ide_channel *channel, uint8_t cmd) {
	// 只要向硬盘发出了命令便将此标记置为true,
	// 硬盘中断处理程序需要根据它来判断
	channel->expect_intr = true;
	outb(reg_cmd(channel), cmd);
}

// 硬盘读入sector_count个扇区的数据到buf
static void read_sector(struct disk *disk, void *buf, uint8_t sector_count) {
	uint32_t byte_size;
	if(sector_count == 0) {
		// 因为sector_count是8位变量,由主调函数将其赋值时,若为256则会将最高位的1丢掉变为0
		byte_size = 256 * 512;
	} else {
		byte_size = sector_count * 512;
	}
	insw(reg_data(disk->channel), buf, byte_size / 2);
}

// 将buf中sector_count扇区的数据写入硬盘
static void write_sector(struct disk *disk, void *buf, uint8_t sector_count) {
	uint32_t byte_size;
	if(sector_count == 0) {
		// 因为sector_count是8位变量,由主调函数将其赋值时,若为256则会将最高位的1丢掉变为0
		byte_size = 256 * 512;
	} else {
		byte_size = sector_count * 512;
	}
	outsw(reg_data(disk->channel), buf, byte_size / 2);
}

// 等待30秒
static bool busy_wait(struct disk *disk) {
	struct ide_channel *channel = disk->channel;
	int16_t time = 30 * 1000; // 30 * 1000毫秒
	while((time -= 10) >= 0) {
		if(!(inb(reg_status(channel)) & BIT_STAT_BSY)) {
			return inb(reg_status(channel)) & BIT_STAT_DRQ;
		} else {
			mill_sleep(10); // 睡眠10毫秒
		}
	}
	return false;
}

// 从硬盘读取sector_count个扇区到buf
void ide_read(struct disk *disk, uint32_t lba, void *buf, uint32_t sector_count) {
	ASSERT(lba <= MAX_LBA && sector_count > 0);
	lock_acquire(&disk->channel->lock);
	// 1 选择操作的硬盘
	select_disk(disk);
	uint32_t secs_op; // 每次操作的扇区数
	uint32_t secs_done = 0; // 已完成的扇区数
	while(secs_done < sector_count) {
		if((secs_done + 256) < sector_count) {
			secs_op = 256;
		} else {
			secs_op = sector_count - secs_done;
		}
		// 2 写入待读入的扇区数和起始扇区号
		select_sector(disk, lba + secs_done, secs_op);
		// 3 执行的命令写入reg_cmd寄存器
		cmd_out(disk->channel, CMD_READ_SECTOR); // 准备开始读数据
		// 阻塞自己的时机
		// 在硬盘已经开始工作(开始在内部读数据或写数据)后才能阻塞自己,
		// 现在硬盘已经开始忙了,将自己阻塞,
		// 等待硬盘完成读操作后通过中断处理程序唤醒自己
		sema_down(&disk->channel->disk_done);
		// 4 检测硬盘状态是否可读
		// 醒来后开始执行下面代码
		if(!busy_wait(disk)) { // 若失败
			char error[64];
			sprintf(error, "%s read sector %d failed!!!\n", disk->name, lba);
			PANIC(error);
		}
		// 5 把数据从硬盘的缓冲区读出
		read_sector(disk, (void*) ((uint32_t) buf + secs_done * 512), secs_op);
		secs_done += secs_op;
	}
	lock_release(&disk->channel->lock);
}

// 将buf中sector_count个扇区数据写入硬盘
void ide_write(struct disk *disk, uint32_t lba, void *buf, uint32_t sector_count) {
	ASSERT(lba <= MAX_LBA && sector_count > 0);
	lock_acquire(&disk->channel->lock);
	// 1 选择操作的硬盘
	select_disk(disk);
	uint32_t secs_op; // 每次操作的扇区数
	uint32_t secs_done = 0; // 已完成的扇区数
	while(secs_done < sector_count) {
		if((secs_done + 256) <= sector_count) {
			secs_op = 256;
		} else {
			secs_op = sector_count - secs_done;
		}
		// 2 写入待写入的扇区数和起始扇区号
		select_sector(disk, lba + secs_done, secs_op);
		// 3 执行的命令写入reg_cmd寄存器
		cmd_out(disk->channel, CMD_WRITE_SECTOR); // 准备开始写数据
		// 4 检测硬盘状态是否可读
		if(!busy_wait(disk)) { // 若失败
			char error[64];
			sprintf(error, "%s write sector %d failed!!!\n", disk->name, lba);
			PANIC(error);
		}
		// 5 将数据写入硬盘
		write_sector(disk, (void*) ((uint32_t) buf + secs_done * 512), secs_op);
		// 在硬盘响应期间阻塞自己
		sema_down(&disk->channel->disk_done);
		secs_done += secs_op;
	}
	lock_release(&disk->channel->lock);
}

// 硬盘中断处理程序
void intr_disk_handler(uint8_t irq_no) {
	ASSERT(irq_no == 0x2e || irq_no == 0x2f);
	uint8_t ch_no = irq_no - 0x2e;
	struct ide_channel *channel = &channels[ch_no];
	ASSERT(channel->irq_no == irq_no);
	// 每次读写硬盘时会申请锁,从而保证了同步一致性
	if(channel->expect_intr) {
		channel->expect_intr = false;
		sema_up(&channel->disk_done);
		// 读取状态寄存器使硬盘控制器认为此次的中断已被处理,
		// 从而硬盘可以继续执行新的读写
		inb(reg_status(channel));
	}
}

// 硬盘数据结构初始化
void ide_init(void) {
	printk("ide_init start\n");
	uint8_t hd_count = *((uint8_t*) 0x475); // 获取硬盘的数量
	ASSERT(hd_count > 0);
	channel_count = DIV_ROUND_UP(hd_count, 2); // 向上取整,通过硬盘数获取ide通道数
	struct ide_channel *channel;
	uint8_t channel_no = 0;
	while(channel_no < channel_count) {
		channel = &channels[channel_no];
		sprintf(channel->name, "ide%d", channel_no);
		// 为每个ide通道初始化基址端口和中断向量
		switch(channel_no) {
			case 0:
				channel->base_port = 0x1f0; // ide0基址端口号是0x1f0
				channel->irq_no = 0x20 + 14; // 硬盘,ide0通道的中断向量号
				break;
			case 1:
				channel->base_port = 0x170; // ide1基址端口号是0x170
				channel->irq_no = 0x20 + 15; // ide1的中断向量号
				break;
		}
		channel->expect_intr = false; // 未向硬盘写入指令时不期待硬盘的中断
		lock_init(&channel->lock);
		// 初始化为0,目的是向硬盘控制器请求数据后,
		// 硬盘驱动sema_down此信号量会阻塞线程,
		// 直到硬盘完成后通过发中断,
		// 由中断处理程序将此信号量sema_up唤醒线程
		sema_init(&channel->disk_done, 0);
		register_handler(channel->irq_no, intr_disk_handler);
		++channel_no; // 下一个channel
	}
	printk("ide_init done\n");
}
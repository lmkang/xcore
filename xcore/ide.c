#include "ide.h"
#include "global.h"
#include "debug.h"
#include "stdio.h"
#include "x86.h"
#include "timer.h"
#include "string.h"
#include "memory.h"
#include "list.h"
#include "interrupt.h"
#include "print.h"

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

// reg_status寄存器的一些关键位
#define BIT_STAT_BSY 0x80 // 硬盘忙
#define BIT_STAT_DRDY 0x40 // 驱动器准备好了
#define BIT_STAT_DRQ 0x8 // 数据传输准备好了

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

// 记录总扩展分区的起始lba,初始为0,partition_scan以此为标记
uint32_t ext_lba_start = 0;

// 硬盘主分区和逻辑分区的下标
uint8_t primary_no = 0, logic_no = 0;

// 分区队列
struct list partition_list;

// 构建一个16字节大小的结构体,用来存分区表项
struct partition_table_entry {
	uint8_t bootable; // 是否可引导
	uint8_t start_head; // 起始磁头号
	uint8_t start_sector; // 起始扇区号
	uint8_t start_chs; // 起始柱面号
	uint8_t fs_type; // 分区类型
	uint8_t end_head; // 结束磁头号
	uint8_t end_sector; // 结束扇区号
	uint8_t end_chs; // 结束柱面号
	uint32_t lba_start; // 本分区起始扇区的lba地址
	uint32_t sector_count; // 本分区扇区数
}__attribute__((packed));

// 引导扇区,MBR或EBR所在的扇区
struct boot_sector {
	uint8_t boot_code[446]; // 引导代码
	struct partition_table_entry partition_table[4]; // 分区表
	uint16_t signature; // 结束标志 0x55,0xaa
}__attribute__((packed));

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
	uint16_t millseconds = 30 * 1000; // 30s
	while(millseconds > 0) {
		if(!(inb(reg_status(channel)) & BIT_STAT_BSY)) {
			return (inb(reg_status(channel)) & BIT_STAT_DRQ);
		} else {
			sleep(10);
		}
		millseconds -= 10;
	}
	return false;
}

// 从硬盘读取sector_count个扇区到buf
void ide_read(struct disk *disk, uint32_t lba_start, void *buf, uint32_t sector_count) {
	ASSERT((lba_start <= MAX_LBA) && (sector_count > 0));
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
		// 2 写入待读入的扇区数和起始扇区号
		select_sector(disk, lba_start + secs_done, secs_op);
		// 3 执行的命令写入reg_cmd寄存器
		out_cmd(disk->channel, CMD_READ_SECTOR); // 准备开始读数据
		// 硬盘开始工作后阻塞自己
		// 等待硬盘完成读操作后通过中断处理程序唤醒自己
		sema_down(&disk->channel->disk_done);
		// 4 检测硬盘状态是否可读
		if(!busy_wait(disk)) { // 失败
			char error[64];
			sprintf(error, "%s read sector %d failed!\n", disk->name, lba_start);
			PANIC(error);
		}
		// 5 把数据从硬盘的缓冲区中读出
		read_sector(disk, (void*) ((uint32_t) buf + secs_done * 512), secs_op);
		secs_done += secs_op;
	}
	lock_release(&disk->channel->lock);
}

// 将buf中sector_count个扇区数据写入硬盘
void ide_write(struct disk *disk, uint32_t lba_start, void *buf, uint32_t sector_count) {
	ASSERT((lba_start <= MAX_LBA) && (sector_count > 0));
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
		select_sector(disk, lba_start + secs_done, secs_op);
		// 3 执行的命令写入reg_cmd寄存器
		out_cmd(disk->channel, CMD_WRITE_SECTOR); // 准备开始写数据
		// 4 检测硬盘状态是否可读
		if(!busy_wait(disk)) { // 失败
			char error[64];
			sprintf(error, "%s write sector %d failed!\n", disk->name, lba_start);
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
	ASSERT((irq_no == 0x2e) || (irq_no == 0x2f));
	uint8_t channel_no = irq_no - 0x2e;
	struct ide_channel *channel = &channels[channel_no];
	ASSERT(channel->irq_no == irq_no);
	// 每次读写硬盘时会申请锁,从而保证了同步一致性
	if(channel->expect_intr) {
		channel->expect_intr = false;
		sema_up(&channel->disk_done);
		// 读取状态寄存器使硬盘控制器认为此次中断已被处理
		// 从而硬盘可以继续执行新的读写
		inb(reg_status(channel));
	}
}

// 将dest中len个相邻字节交换位置后存入buf
static void swap_pair_bytes(const char *dest, char *buf, uint32_t len) {
	uint8_t i;
	for(i = 0; i < len; i += 2) {
		buf[i + 1] = *dest++;
		buf[i] = *dest++;
	}
	buf[i] = '\0';
}

// 获得硬盘参数信息
static void identify_disk(struct disk *disk) {
	char id_info[512];
	select_disk(disk);
	out_cmd(disk->channel, CMD_IDENTIFY);
	sema_down(&disk->channel->disk_done);
	if(!busy_wait(disk)) { // 失败
		char error[64];
		sprintf(error, "%s identify failed!\n", disk->name);
		PANIC(error);
	}
	read_sector(disk, id_info, 1);
	char buf[64];
	uint8_t sn_start = 10 * 2;
	uint8_t sn_len = 20;
	uint8_t md_start = 27 *  2;
	uint8_t md_len = 40;
	swap_pair_bytes(&id_info[sn_start], buf, sn_len);
	printk("disk %s info : \nSN : %s\n", disk->name, buf);
	memset(buf, 0, sizeof(buf));
	swap_pair_bytes(&id_info[md_start], buf, md_len);
	printk("MODULE : %s\n", buf);
	uint32_t sectors = *((uint32_t*) &id_info[60 * 2]);
	printk("SECTORS : %d\n", sectors);
	printk("CAPACITY : %dMB\n", sectors * 512 / 1024 / 1024);
}

// 扫描硬盘disk中地址为ext_lba的扇区中的所有分区
static void partition_scan(struct disk *disk, uint32_t ext_lba) {
	struct boot_sector *bs = sys_malloc(sizeof(struct boot_sector));
	ide_read(disk, ext_lba, bs, 1);
	uint8_t part_index = 0;
	struct partition_table_entry *part_ent = bs->partition_table;
	// 遍历分区表4个分区表项
	while(part_index++ < 4) {
		if(part_ent->fs_type == 0x5) { // 扩展分区
			if(ext_lba_start != 0) {
				// 子扩展分区的lba_start是相对于主引导扇区中的总扩展分区地址
				partition_scan(disk, part_ent->lba_start + ext_lba_start);
			} else {
				// ext_lba_start为0表示是第一次读取引导块,即主引导扇区
				ext_lba_start = part_ent->lba_start;
				partition_scan(disk, part_ent->lba_start);
			}
		} else if(part_ent->fs_type != 0) { // 有效的分区类型
			if(ext_lba == 0) { // 主分区
				disk->primary_parts[primary_no].lba_start = ext_lba + part_ent->lba_start;
				disk->primary_parts[primary_no].sector_count = part_ent->sector_count;
				disk->primary_parts[primary_no].disk = disk;
				list_append(&partition_list, &disk->primary_parts[primary_no].part_tag);
				sprintf(disk->primary_parts[primary_no].name, "%s%d", disk->name, primary_no + 1);
				++primary_no;
				ASSERT(primary_no < 4);
			} else {
				disk->logic_parts[logic_no].lba_start = ext_lba + part_ent->lba_start;
				disk->logic_parts[logic_no].sector_count = part_ent->sector_count;
				disk->logic_parts[logic_no].disk = disk;
				list_append(&partition_list, &disk->logic_parts[logic_no].part_tag);
				sprintf(disk->logic_parts[logic_no].name, "%s%d", disk->name, logic_no + 5);
				++logic_no;
				if(logic_no >= 8) { // 只支持8个逻辑分区
					return;
				}
			}
		}
		++part_ent;
	}
	sys_free(bs);
}

// 打印分区信息
static bool partition_info(struct list_ele *ele, __attribute__((unused)) int arg) {
	struct partition *part = ELE2ENTRY(struct partition, part_tag, ele);
	printk("%s lba_start : %x, sector_count : %x\n", 
		part->name, part->lba_start, part->sector_count);
	// 为了让主调函数list_traversal继续往下遍历
	return false;
}

// 硬盘数据结构初始化
void ide_init(void) {
	// 获取硬盘数
	uint8_t disk_count = *((uint8_t*) P2V(0x475));
	ASSERT(disk_count > 0);
	list_init(&partition_list);
	// 一个ide通道有两个硬盘,根据硬盘数量算出ide通道数
	channel_count = DIV_ROUND_UP(disk_count, 2);
	struct ide_channel *channel;
	uint8_t channel_no = 0, dev_no = 0;
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
		register_intr_handler(channel->irq_no, intr_disk_handler);
		// 获取硬盘的参数及分区信息,目前就一个硬盘
		while(dev_no < 1) {
			struct disk *disk = &channel->devices[dev_no];
			disk->channel = channel;
			disk->dev_no = dev_no;
			sprintf(disk->name, "sd%c", 'a' + channel_no * 2 + dev_no);
			identify_disk(disk); // 获取硬盘参数
			partition_scan(disk, 0);
			primary_no = 0;
			logic_no = 0;
			++dev_no;
		}
		dev_no = 0; // 将硬盘驱动器号置0
		++channel_no;
	}
	printk("\n--------- all partition info ---------\n");
	// 打印所有分区信息
	list_traversal(&partition_list, partition_info, (int) NULL);
	printk("ide_init done\n");
}



















































































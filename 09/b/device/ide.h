#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H
#include "stdint.h"
#include "list.h"
#include "bitmap.h"
#include "global.h"
#include "sync.h"

// 分区结构
struct partition {
	uint32_t start_lba; // 起始扇区
	uint32_t sector_count; // 扇区数
	struct disk *disk; // 分区所属硬盘
	struct list_ele part_tag; // 用于队列中的标记
	char name[8]; // 分区名称
	struct super_block *sp_block; // 分区的超级块
	struct bitmap block_btmp; // 块位图
	struct bitmap inode_btmp; // inode位图
	struct list open_inodes; // 本分区打开的inode队列
};

// 硬盘结构
struct disk {
	char name[8]; // 硬盘名称
	struct ide_channel *channel; // 硬盘所属ide通道
	uint8_t dev_no; // 硬盘是主0,还是从1
	struct partition primary_parts[4]; // 主分区最多4个
	struct partition logic_parts[8]; // 逻辑分区数量无限,这里支持8个
};

// ata通道结构
struct ide_channel {
	char name[8]; // ata通道名称
	uint16_t base_port; // 通道的基址端口号
	uint8_t irq_no; // 通道所用的中断号
	struct lock lock; // 通道锁
	bool expect_intr; // 表示等待硬盘的中断
	struct semaphore disk_done; // 用于阻塞,唤醒驱动程序
	struct disk devices[2]; // 一个通道上连接两个硬盘,一主一从
};

void ide_init(void);

void intr_disk_handler(uint8_t irq_no);

void ide_read(struct disk *disk, uint32_t lba, void *buf, uint32_t sector_count);

void ide_write(struct disk *disk, uint32_t lba, void *buf, uint32_t sector_count);

#endif
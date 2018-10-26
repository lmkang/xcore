#ifndef __IDE_H
#define __IDE_H

#include "types.h"
#include "list.h"
#include "bitmap.h"
#include "sync.h"

// 分区结构
struct partition {
	uint32_t lba_start; // 起始扇区
	uint32_t sector_count; // 扇区数
	struct disk *disk; // 分区所属的硬盘
	struct list_ele part_tag; // 用于队列中的标记
	char name[8]; // 分区名称
	struct super_block *sp_block; // 本分区的超级块
	struct bitmap block_btmp; // 块位图
	struct bitmap inode_btmp; // inode位图
	struct list open_inodes; // 本分区打开的inode队列
};

// 硬盘结构
struct disk {
	char name[8]; // 硬盘名称
	struct ide_channel *channel; // 硬盘所属ide通道
	uint8_t dev_no; // 主硬盘0,从硬盘1
	struct partition primary_parts[4]; // 主分区最多4个
	struct partition logic_parts[8]; // 逻辑分区支持8个
};

// ata通道结构
struct ide_channel {
	char name[8]; // ata通道名称
	uint16_t port_base; // 起始端口号
	uint8_t irq_no; // 中断号
	struct lock lock; // 通道锁
	bool expect_intr; // 表示等待硬盘的中断
	struct semaphore disk_done; // 用于阻塞和唤醒驱动程序
	struct disk devices[2]; // 一个通道连接两个硬盘,主和从
};

#endif
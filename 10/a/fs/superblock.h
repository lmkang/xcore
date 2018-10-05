#ifndef __FS_SUPERBLOCK_H
#define __FS_SUPERBLOCK_H
#include "stdint.h"

// 超级块
struct super_block {
	uint32_t magic; // 用来标识文件系统类型
	uint32_t sector_count; // 分区总扇区数
	uint32_t inode_count; // 分区inode数量
	uint32_t part_lba_base; // 分区的起始lba地址
	uint32_t block_btmp_lba; // 块位图本身起始扇区地址
	uint32_t block_btmp_secs; // 块位图占用的扇区数量
	uint32_t inode_btmp_lba; // inode位图起始扇区lba地址
	uint32_t inode_btmp_secs; // inode位图占用的扇区数量
	uint32_t inode_table_lba; // inode表起始扇区lba地址
	uint32_t inode_table_secs; // inode表占用的扇区数量
	uint32_t data_start_lba; // 数据区的起始扇区lba地址
	uint32_t root_inode_no; // 根目录所在的inode编号
	uint32_t dir_entry_size; // 目录项大小
	uint8_t pad[460]; // 加上460字节,凑够512字节,1扇区的大小
}__attribute__((packed));

#endif
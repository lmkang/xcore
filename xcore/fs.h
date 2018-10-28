#ifndef __FS_H
#define __FS_H

#include "types.h"
#include "list.h"

#define MAX_FILENAME_LEN 16 // 最大文件名长度

#define MAX_FILE_COUNT 4096 // 每个分区所支持最大创建的文件数

#define SECTOR_BIT_COUNT 4096 // 每个扇区的位数

#define SECTOR_SIZE 512 // 扇区字节大小

#define BLOCK_SIZE 512 // 块字节大小

// 文件类型
enum file_type {
	FT_UNKNOWN, // 不支持的文件类型
	FT_FILE, // 普通文件
	FT_DIRECTORY // 目录
};

// 超级块
struct super_block {
	uint32_t magic; // 用来标识文件系统类型
	uint32_t sector_count; // 本分区总共的扇区数
	uint32_t inode_count; // 本分区inode的数量
	uint32_t part_lba_start; // 本分区的起始lba地址
	uint32_t block_btmp_lba; // 块位图起始扇区地址
	uint32_t block_btmp_secs; // 块位图占用的扇区数量
	uint32_t inode_btmp_lba; // inode位图起始扇区地址
	uint32_t inode_btmp_secs; // inode位图占用的扇区数量
	uint32_t inode_table_lba; // inode表起始扇区地址
	uint32_t inode_table_secs; // inode表占用的扇区数量
	uint32_t data_lba_start; // 数据区的起始扇区号
	uint32_t root_inode_no; // 根目录所在的inode号
	uint32_t dir_entry_size; // 目录项大小
	uint8_t pad[460]; // 加上460字节,凑够512字节1扇区大小
}__attribute__((packed));

// inode结构
struct inode {
	uint32_t i_no; // inode编号
	uint32_t i_size; // 文件大小或所有目录项大小之和
	uint32_t open_count; // 记录此文件被打开的次数
	bool write_flag; // 写文件不能并行,进程写文件前检查此标识
	uint32_t sectors[13]; // sectors[0-11]是直接块,sectors[12]用来存储一级间接块指针
	struct list_ele inode_tag;
};

// 目录结构
struct directory {
	struct inode *inode;
	uint32_t dir_pos; // 记录在目录内的偏移
	uint8_t dir_buf[512]; // 目录的数据缓存
};

// 目录项结构
struct dir_entry {
	char filename[MAX_FILENAME_LEN]; // 普通文件或目录名称
	uint32_t i_no; // inode编号
	enum file_type f_type; // 文件类型
};

void fs_init(void);

#endif










































































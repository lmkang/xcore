#ifndef __INODE_H
#define __INODE_H

#include "types.h"
#include "ide.h"

// inode结构
struct inode {
	uint32_t i_no; // inode编号
	uint32_t i_size; // 文件大小或所有目录项大小之和
	uint32_t open_count; // 记录此文件被打开的次数
	bool write_flag; // 写文件不能并行,进程写文件前检查此标识
	uint32_t sectors[13]; // sectors[0-11]是直接块,sectors[12]用来存储一级间接块指针
	struct list_ele inode_tag;
};

void inode_sync(struct partition *part, struct inode *inode, void *io_buf);

struct inode *inode_open(struct partition *part, uint32_t inode_no);

void inode_close(struct inode *inode);

void inode_delete(struct partitin *part, uint32_t inode_no, void *buf);

void inode_release(struct partition *part, uint32_t inode_no);

void inode_init(uint32_t inode_no, struct inode *inode);

#endif
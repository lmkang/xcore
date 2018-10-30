#ifndef __FILE_H
#define __FILE_H

#include "types.h"
#include "inode.h"
#include "directory.h"

#define MAX_FILE_OPEN 32 // 系统可打开的最大文件数

// 文件结构
struct file {
	// 记录当前文件操作的偏移地址,以0起始,最大为文件大小-1
	uint32_t fd_pos;
	uint32_t fd_flag;
	struct inode *fd_inode;
};

// 标准输入输出描述符
enum std_fd {
	STDIN_FD, // 0 标准输入
	STDOUT_FD, // 1 标准输出
	STDERR_FD // 2 标准错误
};

// 位图类型
enum bitmap_type {
	INODE_BITMAP, // inode位图
	BLOCK_BITMAP // 块位图
};

int32_t get_free_slot(void);

int32_t install_task_fd(int32_t global_fd_index);

int32_t alloc_inode_bitmap(struct partition *part);

int32_t alloc_block_bitmap(struct partition *part);

void bitmap_sync(struct partition *part, uint32_t bit_index, enum bitmap_type btmp);

int32_t file_create(struct directory *parent_dir, char *filename, uint8_t flag);

int32_t file_open(uint32_t inode_no, uint8_t flag);

int32_t file_close(struct file *file);

int32_t file_write(struct file *file, const void *buf, uint32_t count);

int32_t file_read(struct file *file, void *buf, uint32_t count);

#endif
#ifndef __DIRECTORY_H
#define __DIRECTORY_H

#include "types.h"
#include "fs.h"

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

void open_root_dir(struct partition *part);

struct directory *dir_open(struct partition *part, uint32_t inode_no);

bool search_dir_entry(struct partition *part, struct directory *dir, \
	const char *name, struct dir_entry *dir_ent);

void dir_close(struct directory *dir);

void create_dir_entry(char *filename, uint32_t inode_no, \
	enum file_type f_type, struct dir_entry *dir_ent);

bool sync_dir_entry(struct directory *parent_dir, \
	struct dir_entry *dir_ent, void *io_buf);

bool delete_dir_entry(struct partition *part, struct directory *dir, \
	uint32_t inode_no, void *io_buf);

#endif
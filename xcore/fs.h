#ifndef __FS_H
#define __FS_H

#include "types.h"
#include "list.h"
#include "inode.h"

#define MAX_FILENAME_LEN 16 // 最大文件名长度

#define MAX_FILE_COUNT 4096 // 每个分区所支持最大创建的文件数

#define SECTOR_BIT_COUNT 4096 // 每个扇区的位数

#define SECTOR_SIZE 512 // 扇区字节大小

#define BLOCK_SIZE 512 // 块字节大小

#define MAX_PATH_LEN 512 // 路径最大长度

// 文件类型
enum file_type {
	FT_UNKNOWN, // 不支持的文件类型
	FT_FILE, // 普通文件
	FT_DIRECTORY // 目录
};

// 打开文件的选项
enum file_option {
	FO_READONLY, // 只读
	FO_WRITEONLY, // 只写
	FO_READWRITE, // 读写
	FO_CREATE = 4 // 创建
};

// 文件读写位置偏移量
enum whence {
	SEEK_SET = 1,
	SEEK_CUR,
	SEEK_END
};

// 用来记录查找文件过程中已找到的上级路径
struct path_record {
	char searched_path[MAX_PATH_LEN]; // 查找过程中的父路径
	struct directory *parent_dir; // 文件或目录所在的直接父目录
	enum file_type f_type; // 文件类型
};

// 文件属性结构体
struct file_stat {
	uint32_t i_no; // inode编号
	uint32_t size; // 尺寸
	enum file_type f_type; // 文件类型
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

int32_t get_path_depth(char *pathname);

int32_t sys_open(const char *pathname, enum file_option f_opt);

int32_t sys_close(int32_t fd);

int32_t sys_write(int32_t fd, const void *buf, uint32_t count);

int32_t sys_read(int32_t fd, void *buf, uint32_t count);

int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);

int32_t sys_unlink(const char *pathname);

int32_t sys_mkdir(const char *pathname);

struct directory *sys_opendir(const char *name);

int32_t sys_closedir(struct directory *dir);

struct dir_entry *sys_readdir(struct directory *dir);

void sys_rewinddir(struct directory *dir);

int32_t sys_rmdir(const char *pathname);

char *sys_getcwd(char *buf, uint32_t size);

int32_t sys_chdir(const char *path);

int32_t sys_stat(const char *path, struct file_stat *buf);

void fs_init(void);

#endif










































































#ifndef __FS_FS_H
#define __FS_FS_H

#define MAX_FILES_PER_PART 4096 // 每个分区支持的最大创建的文件数
#define BITS_PER_SECTOR 4096 // 每扇区的位数
#define SECTOR_SIZE 512 // 扇区字节大小
#define BLOCK_SIZE 512 // 块字节大小

enum file_type {
	FT_UNKNOWN, // 不支持的文件类型
	FT_REGULAR, // 普通文件
	FT_DIRECTORY // 目录
};

void fs_init();

#endif
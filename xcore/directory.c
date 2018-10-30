#include "directory.h"
#include "memory.h"
#include "fs.h"
#include "debug.h"
#include "string.h"
#include "file.h"
#include "print.h"

// 默认情况下操作的分区
extern struct partition *cur_part;

// 根目录
struct directory root_dir;

// 打开根目录
void open_root_dir(struct partition *part) {
	root_dir.inode = inode_open(part, part->sp_block->root_inode_no);
	root_dir.dir_pos = 0;
}

// 在分区part上打开inode编号为inode_no的目录并返回目录指针
struct directory *dir_open(struct partition *part, uint32_t inode_no) {
	struct directory *dir = (struct directory*) sys_malloc(sizeof(struct directory));
	dir->inode = inode_open(part, inode_no);
	dir->dir_pos = 0;
	return dir;
}

// 在part分区内的dir目录内寻找名为name的文件或目录,
// 找到后返回true并将其目录项存入dir_ent,否则返回false
bool search_dir_entry(struct partition *part, struct directory *dir, \
	const char *name, struct dir_entry *dir_ent) {
	uint32_t block_count = 140; // 12个直接块+128个一级间接块
	uint32_t *all_blocks = (uint32_t*) sys_malloc(48 + 512);
	if(all_blocks == NULL) {
		printk("search_dir_entry : alloc memory failed!\n");
		return false;
	}
	uint32_t block_index = 0;
	while(block_index < 12) {
		all_blocks[block_index] = dir->inode->sectors[block_index];
		++block_index;
	}
	block_index = 0;
	if(dir->inode->sectors[12] != 0) { // 有一级间接块表
		ide_read(part->disk, dir->inode->sectors[12], all_blocks + 12, 1);
	}
	// 此时,all_blocks存储的是该文件或目录的所有扇区地址
	// 写目录项的时候已保证目录项不跨扇区,
	// 只申请1个扇区的内存
	uint8_t *buf = (uint8_t*) sys_malloc(SECTOR_SIZE);
	struct dir_entry *p_dir_ent = (struct dir_entry*) buf;
	uint32_t dir_entry_size = part->sp_block->dir_entry_size;
	uint32_t dir_entry_count = SECTOR_SIZE / dir_entry_size; // 1扇区内可容纳的目录项个数
	// 开始在所有块中查找目录项
	while(block_index < block_count) {
		// 块地址为0时表示该块中无数据
		if(all_blocks[block_index] == 0) {
			++block_index;
			continue;
		}
		ide_read(part->disk, all_blocks[block_index], buf, 1);
		uint32_t dir_entry_index = 0;
		// 遍历扇区中所有目录项
		while(dir_entry_index < dir_entry_count) {
			// 找到就直接复制整个目录项
			printk("p_dir_ent->filename : %s, name : %s\n", p_dir_ent->filename, name);
			if(!strcmp(p_dir_ent->filename, name)) {
				memcpy(dir_ent, p_dir_ent, dir_entry_size);
				sys_free(buf);
				sys_free(all_blocks);
				return true;
			}
			++dir_entry_index;
			++p_dir_ent;
		}
		++block_index;
		p_dir_ent = (struct dir_entry*) buf;
		memset(buf, 0, SECTOR_SIZE);
	}
	sys_free(buf);
	sys_free(all_blocks);
	return false;
}

// 关闭目录
void dir_close(struct directory *dir) {
	// 根目录不能关闭
	if(dir == &root_dir) {
		return;
	}
	inode_close(dir->inode);
	sys_free(dir);
}

// 在内存中初始化目录项dir_ent
void create_dir_entry(char *filename, uint32_t inode_no, \
	enum file_type f_type, struct dir_entry *dir_ent) {
	ASSERT(strlen(filename) <= MAX_FILENAME_LEN);
	memcpy(dir_ent->filename, filename, strlen(filename));
	dir_ent->i_no = inode_no;
	dir_ent->f_type = f_type;
}

// 将目录项dir_ent写入父目录parent_dir中,io_buf由主调函数提供
bool sync_dir_entry(struct directory *parent_dir, \
	struct dir_entry *dir_ent, void *io_buf) {
	struct inode *dir_inode = parent_dir->inode;
	uint32_t dir_size = dir_inode->i_size;
	uint32_t dir_entry_size = cur_part->sp_block->dir_entry_size;
	ASSERT(dir_size % dir_entry_size == 0);
	uint32_t dir_entry_count = (512 / dir_entry_size);
	int32_t block_lba = -1;
	// 将该目录的所有扇区地址存入all_blocks
	uint8_t block_index = 0;
	uint32_t all_blocks[140] = {0};
	// 将12个直接块存入all_blocks
	while(block_index < 12) {
		all_blocks[block_index] = dir_inode->sectors[block_index];
		++block_index;
	}
	struct dir_entry *p_dir_ent = (struct dir_entry*) io_buf;
	int32_t block_btmp_index = -1;
	// 开始遍历所有块以寻找目录项空位,若已有扇区中没有空闲位,
	// 在不超过文件大小的情况下申请新扇区来存储新目录项
	block_index = 0;
	// 文件(包括目录)最大支持12块 + 128个间接块 = 140块
	while(block_index < 140) {
		block_btmp_index = -1;
		if(all_blocks[block_index] == 0) {
			block_lba = alloc_block_bitmap(cur_part);
			if(block_lba == -1) {
				printk("alloc_block_bitmap failed!\n");
				return false;
			}
			// 每分配一个块就同步一次block_bitmap
			block_btmp_index = block_lba - cur_part->sp_block->data_lba_start;
			ASSERT(block_btmp_index != -1);
			bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
			block_btmp_index = -1;
			if(block_index < 12) { // 直接块
				dir_inode->sectors[block_index] = block_lba;
				all_blocks[block_index] = block_lba;
			} else if(block_index == 12) { // 若是尚未分配一级间接块表
				dir_inode->sectors[12] = block_lba;
				block_lba = -1;
				block_lba = alloc_block_bitmap(cur_part);
				// 再分配一个块作为第0个间接块
				if(block_lba == -1) {
					block_btmp_index = dir_inode->sectors[12] - cur_part->sp_block->data_lba_start;
					set_bitmap(&cur_part->block_btmp, block_btmp_index, 0);
					dir_inode->sectors[12] = 0;
					printk("alloc_block_bitmap failed!\n");
					return false;
				}
				// 每分配一个块就同步一次block_btmp
				block_btmp_index = block_lba - cur_part->sp_block->data_lba_start;
				ASSERT(block_btmp_index != -1);
				bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
				all_blocks[12] = block_lba;
				// 把新分配的第0个间接块地址写入一级间接块表
				ide_write(cur_part->disk, dir_inode->sectors[12], all_blocks + 12, 1);
			} else { // 间接块未分配
				all_blocks[block_index] = block_lba;
				// 把新分配的第(block_index - 12)个间接块地址写入一级间接块表
				ide_write(cur_part->disk, dir_inode->sectors[12], all_blocks + 12, 1);
			}
			// 再将新目录项p_dir_ent写入新分配的间接块
			memset(io_buf, 0, 512);
			memcpy(io_buf, p_dir_ent, dir_entry_size);
			ide_write(cur_part->disk, all_blocks[block_index], io_buf, 1);
			dir_inode->i_size += dir_entry_size;
			return true;
		}
		// 若第block_index块已存在,将其读进内存,然后在该块查找空目录项
		ide_read(cur_part->disk, all_blocks[block_index], io_buf, 1);
		// 在扇区内查找空目录项
		for(uint8_t dir_entry_index = 0; dir_entry_index < dir_entry_count; dir_entry_index++) {
			if((dir_ent + dir_entry_index)->f_type == FT_UNKNOWN) {
				// 无论是初始化或删除文件后,都将f_type置为FT_UNKNOWN
				memcpy(dir_ent + dir_entry_index, p_dir_ent, dir_entry_size);
				ide_write(cur_part->disk, all_blocks[block_index], io_buf, 1);
				dir_inode->i_size += dir_entry_size;
				return true;
			}
		}
		++block_index;
	}
	printk("directory is full!\n");
	return false;
}



















































































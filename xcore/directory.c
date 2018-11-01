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
	uint32_t dir_entry_count = (SECTOR_SIZE / dir_entry_size);
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
			memcpy(io_buf, dir_ent, dir_entry_size);
			ide_write(cur_part->disk, all_blocks[block_index], io_buf, 1);
			dir_inode->i_size += dir_entry_size;
			return true;
		}
		// 若第block_index块已存在,将其读进内存,然后在该块查找空目录项
		ide_read(cur_part->disk, all_blocks[block_index], io_buf, 1);
		// 在扇区内查找空目录项
		for(uint8_t dir_entry_index = 0; dir_entry_index < dir_entry_count; dir_entry_index++) {
			if((p_dir_ent + dir_entry_index)->f_type == FT_UNKNOWN) {
				// 无论是初始化或删除文件后,都将f_type置为FT_UNKNOWN
				memcpy(p_dir_ent + dir_entry_index, dir_ent, dir_entry_size);
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

// 把分区part目录dir中编号为inode_no的目录项删除
bool delete_dir_entry(struct partition *part, struct directory *dir, \
	uint32_t inode_no, void *io_buf) {
	struct inode *dir_inode = dir->inode;
	uint32_t block_index = 0;
	uint32_t all_blocks[140] = {0};
	// 收集目录全部的块地址
	while(block_index < 12) {
		all_blocks[block_index] = dir_inode->sectors[block_index];
		++block_index;
	}
	if(dir_inode->sectors[12]) {
		ide_read(part->disk, dir_inode->sectors[12], all_blocks + 12, 1);
	}
	// 目录项在存储时保证不会跨扇区
	uint32_t dir_entry_size = part->sp_block->dir_entry_size;
	// 1个扇区最大的目录项数目
	uint32_t dir_entry_count = SECTOR_SIZE / dir_entry_size;
	struct dir_entry *dir_ent = (struct dir_entry*) io_buf;
	struct dir_entry *dir_ent_found = NULL;
	uint8_t dir_entry_index;
	uint8_t dir_entry_cnt;
	bool is_first_block; // 是否是目录的第1个块
	// 遍历所有块,寻找目录项
	block_index = 0;
	while(block_index < 140) {
		is_first_block = false;
		if(all_blocks[block_index] == 0) {
			++block_index;
			continue;
		}
		dir_entry_index = dir_entry_cnt = 0;
		memset(io_buf, 0, SECTOR_SIZE);
		// 读取扇区,获得目录项
		ide_read(part->disk, all_blocks[block_index], io_buf, 1);
		// 遍历所有的目录项
		// 统计该扇区的目录项数量和是否有待删除的目录项
		while(dir_entry_index < dir_entry_count) {
			if((dir_ent + dir_entry_index)->f_type != FT_UNKNOWN) {
				if(!strcmp((dir_ent + dir_entry_index)->filename, ".")) {
					is_first_block = true;
				} else if(strcmp((dir_ent + dir_entry_index)->filename, ".")
					&& strcmp((dir_ent + dir_entry_index)->filename, "..")) {
					++dir_entry_cnt;
					// 统计此扇区内的目录项个数,用来判断删除目录项后是否回收该扇区
					if((dir_ent + dir_entry_index)->i_no == inode_no) {
						// 如果找到inode,就将其记录在dir_ent_found
						ASSERT(dir_ent_found == NULL);
						dir_ent_found = dir_ent + dir_entry_index;
					}
				}
			}
			++dir_entry_index;
		}
		// 若此扇区未找到该目录项,继续在下个扇区中找
		if(dir_ent_found == NULL) {
			++block_index;
			continue;
		}
		// 在此扇区中找到目录项后,清除该目录项,
		// 判断是否回收扇区,然后直接返回
		ASSERT(dir_entry_cnt >= 1);
		// 除目录第1个扇区外,若该扇区只有该目录项自己,
		// 则回收整个扇区
		if(dir_entry_cnt == 1 && !is_first_block) {
			// a 在块位图中回收该块
			uint32_t block_btmp_index = all_blocks[block_index] - \
				part->sp_block->data_lba_start;
			set_bitmap(&part->block_btmp, block_btmp_index, 0);
			bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
			// b 将块地址从数组sectors或索引表中去掉
			if(block_index < 12) {
				dir_inode->sectors[block_index] = 0;
			} else { // 在一级间接索引表中擦除该间接块地址
				// 先判断一级间接索引表间接块的数量,
				// 如果只有1个间接块,连同间接索引表所在的块一同回收
				uint32_t indirect_blocks = 0;
				uint32_t indirect_block_index = 12;
				while(indirect_block_index < 140) {
					if(all_blocks[indirect_block_index] != 0) {
						++indirect_blocks;
					}
				}
				ASSERT(indirect_blocks >= 1);
				if(indirect_blocks > 1) {
					all_blocks[block_index] = 0;
					ide_write(part->disk, dir_inode->sectors[12], all_blocks + 12, 1);
				} else {
					// 间接索引表就当前1个间接块
					// 直接回收所在块,然后擦除间接索引表块地址
					block_btmp_index = dir_inode->sectors[12] - \
						part->sp_block->data_lba_start;
					set_bitmap(&part->block_btmp, block_btmp_index, 0);
					bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
					// 将间接索引表清0
					dir_inode->sectors[12] = 0;
				}
			}
		} else { // 仅将该目录项清空
			memset(dir_ent_found, 0, dir_entry_size);
			ide_write(part->disk, all_blocks[block_index], io_buf, 1);
		}
		// 更新inode信息并同步到硬盘
		ASSERT(dir_inode->i_size >= dir_entry_size);
		dir_inode->i_size = dir_entry_size;
		memset(io_buf, 0, SECTOR_SIZE * 2);
		inode_sync(part, dir_inode, io_buf);
		return true;
	}
	// 所有块中未找到则返回false,此时应该是searche_file出错了
	return false;
}

// 读取目录,成功返回1个目录项,失败返回NULL
struct dir_entry *dir_read(struct directory *dir) {
	struct dir_entry *dir_ent = (struct dir_entry*) dir->dir_buf;
	struct inode *dir_inode = dir->inode;
	uint32_t all_blocks[140] = {0};
	uint32_t block_cnt = 12;
	uint32_t block_index = 0;
	uint32_t dir_entry_index = 0;
	while(block_index < 12) {
		all_blocks[block_index] = dir_inode->sectors[block_index];
		++block_index;
	}
	if(dir_inode->sectors[12] != 0) { // 有一级间接块表
		ide_read(cur_part->disk, dir_inode->sectors[12], all_blocks + 12, 1);
		block_cnt = 140;
	}
	block_index = 0;
	uint32_t cur_dir_entry_pos = 0;
	// 当前目录项的偏移,此项用来判断是否是之前已经返回过的目录项
	uint32_t dir_entry_size = cur_part->sp_block->dir_entry_size;
	// 1扇区内可容纳的目录项个数
	uint32_t dir_entry_count = SECTOR_SIZE / dir_entry_size;
	// 在目录大小内遍历
	while(dir->dir_pos < dir_inode->i_size) {
		if(dir->dir_pos >= dir_inode->i_size) {
			return NULL;
		}
		if(all_blocks[block_index] == 0) {
			// 如果此块地址为0,即空块,继续读出下一块
			++block_index;
			continue;
		}
		memset(dir_ent, 0, SECTOR_SIZE);
		ide_read(cur_part->disk, all_blocks[block_index], dir_ent, 1);
		dir_entry_index = 0;
		// 遍历扇区内所有目录项
		while(dir_entry_index < dir_entry_count) {
			if((dir_ent + dir_entry_index)->f_type != FT_UNKNOWN) {
				// 判断是不是最新的目录项
				// 避免返回曾经已经返回过的目录项
				if(cur_dir_entry_pos < dir->dir_pos) {
					cur_dir_entry_pos += dir_entry_size;
					++dir_entry_index;
					continue;
				}
				ASSERT(cur_dir_entry_pos == dir->dir_pos);
				// 更新为新位置,即下一个返回的目录项地址
				dir->dir_pos += dir_entry_size;
				return dir_ent + dir_entry_index;
			}
			++dir_entry_index;
		}
		++block_index;
	}
	return NULL;
}

// 判断目录是否为空
bool dir_empty(struct directory *dir) {
	struct inode *dir_inode = dir->inode;
	// 若目录下只有.和..这两个目录项,则目录为空
	return dir_inode->i_size == cur_part->sp_block->dir_entry_size * 2;
}

// 在父目录parent_dir中删除child_dir
int32_t dir_remove(struct directory *parent_dir, struct directory *child_dir) {
	struct inode *child_dir_inode = child_dir->inode;
	// 空目录只在inode->sectors[0]中有扇区,其他扇区都应该为空
	int32_t block_index = 1;
	while(block_index < 13) {
		ASSERT(child_dir_inode->sectors[block_index] == 0);
		++block_index;
	}
	void *io_buf = sys_malloc(SECTOR_SIZE * 2);
	if(io_buf == NULL) {
		printk("dir_remove : alloc memory failed!\n");
		return -1;
	}
	// 在父目录parent_dir中删除子目录child_dir对应的目录项
	delete_dir_entry(cur_part, parent_dir, child_dir_inode->i_no, io_buf);
	// 回收inode中sectors所占用的扇区
	// 同步inode_btmp和block_btmp
	inode_release(cur_part, child_dir_inode->i_no);
	sys_free(io_buf);
	return 0;
}











































































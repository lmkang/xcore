#include "file.h"
#include "print.h"
#include "bitmap.h"
#include "fs.h"
#include "memory.h"
#include "inode.h"
#include "string.h"
#include "interrupt.h"
#include "ide.h"
#include "debug.h"

// 默认情况下操作的分区
extern struct partition *cur_part;

// 文件表
struct file file_table[MAX_FILE_OPEN];

// 从文件表file_table中获取一个空闲位,成功返回下标,失败返回-1
int32_t get_free_slot(void) {
	uint32_t fd_index;
	for(fd_index = 3; fd_index < MAX_FILE_OPEN; fd_index++) {
		if(file_table[fd_index].fd_inode == NULL) {
			break;
		}
	}
	if(fd_index == MAX_FILE_OPEN) {
		printk("exceed max open files!\n");
		return -1;
	}
	return fd_index;
}

// 将全局描述符小标安装到进程或线程自己的文件描述符数组fd_table中,
// 成功返回下标,失败返回-1
int32_t install_task_fd(int32_t global_fd_index) {
	struct task_struct *cur_thread = current_thread();
	uint8_t local_fd_index;
	for(local_fd_index = 3; local_fd_index < PROC_MAX_FILE_OPEN; local_fd_index++) {
		if(cur_thread->fd_table[local_fd_index] == -1) { // 可用
			cur_thread->fd_table[local_fd_index] = global_fd_index;
			break;
		}
	}
	if(local_fd_index == PROC_MAX_FILE_OPEN) {
		printk("exceed proc max file open!\n");
		return -1;
	}
	return local_fd_index;
}

// 分配一个inode,成功返回inode_no,失败返回-1
int32_t alloc_inode_bitmap(struct partition *part) {
	int32_t bit_index = alloc_bitmap(&part->inode_btmp, 1);
	if(bit_index == -1) {
		return -1;
	}
	return bit_index;
}

// 分配1个扇区,成功返回扇区地址,失败返回-1
int32_t alloc_block_bitmap(struct partition *part) {
	int32_t bit_index = alloc_bitmap(&part->block_btmp, 1);
	if(bit_index == -1) {
		return -1;
	}
	// 此处返回的不是位图索引,而是具体可用的扇区地址
	return part->sp_block->data_lba_start + bit_index;
}

// 将内存中bitmap第bit_index位所在的512字节同步到硬盘
void bitmap_sync(struct partition *part, uint32_t bit_index, enum bitmap_type btmp_type) {
	uint32_t sector_offset = bit_index / 4096;
	uint32_t offset_size = sector_offset * BLOCK_SIZE;
	uint32_t sector_lba;
	uint8_t *btmp_offset;
	// 需要被同步到硬盘的位图只有inode_btp和block_btmp
	switch(btmp_type) {
		case INODE_BITMAP:
			sector_lba = part->sp_block->inode_btmp_lba + sector_offset;
			btmp_offset = part->inode_btmp.bits + offset_size;
			break;
		case BLOCK_BITMAP:
			sector_lba = part->sp_block->block_btmp_lba + sector_offset;
			btmp_offset = part->block_btmp.bits + offset_size;
			break;
	}
	ide_write(part->disk, sector_lba, btmp_offset, 1);
}

// 创建文件,若成功则返回文件描述符,否则返回-1
int32_t file_create(struct directory *parent_dir, char *filename, uint8_t flag) {
	// 后续操作的公共缓冲区
	void *io_buf = sys_malloc(1024);
	if(io_buf == NULL) {
		printk("file_create : sys_malloc failed!\n");
		return -1;
	}
	uint8_t rollback_flag = 0; // 用于操作失败时回滚各资源状态
	// 为新文件分配inode
	int32_t inode_no = alloc_inode_bitmap(cur_part);
	if(inode_no == -1) {
		printk("file_create : alloc_inode_bitmap failed!\n");
		return -1;
	}
	// 此inode要从堆中申请内存,不可生成局部变量(函数退出时会释放)
	// 因为file_table数组中的文件描述符的inode指针要指向它
	struct inode *new_inode = (struct inode*) sys_malloc(sizeof(struct inode));
	if(new_inode == NULL) {
		printk("fil_create : sys_malloc failed!\n");
		rollback_flag = 1;
		goto rollback;
	}
	inode_init(inode_no, new_inode);
	int fd_index = get_free_slot();
	if(fd_index == -1) {
		printk("exceed max open files!\n");
		rollback_flag = 2;
		goto rollback;
	}
	file_table[fd_index].fd_inode = new_inode;
	file_table[fd_index].fd_pos = 0;
	file_table[fd_index].fd_flag = flag;
	file_table[fd_index].fd_inode->write_flag = false;
	struct dir_entry new_dir_entry;
	memset(&new_dir_entry, 0, sizeof(struct dir_entry));
	create_dir_entry(filename, inode_no, FT_FILE, &new_dir_entry);
	// 同步内存数据到硬盘
	// 1 在目录parent_dir下安装目录项new_dir_entry,
	// 写入硬盘后返回true,否则false
	if(!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
		printk("sync_dir_entry to disk failed!\n");
		rollback_flag = 3;
		goto rollback;
	}
	memset(io_buf, 0, 1024);
	// 2 将父目录inode的内容同步到硬盘
	inode_sync(cur_part, parent_dir->inode, io_buf);
	memset(io_buf, 0, 1024);
	// 3 将新创建文件的inode内容同步到硬盘
	inode_sync(cur_part, new_inode, io_buf);
	// 4 将inode_btmp位图同步到硬盘
	bitmap_sync(cur_part, inode_no, INODE_BITMAP);
	// 5 将创建的文件inode添加到open_inodes链表
	list_push(&cur_part->open_inodes, &new_inode->inode_tag);
	new_inode->open_count = 1;
	sys_free(io_buf);
	return install_task_fd(fd_index);
rollback:
	switch(rollback_flag) {
		case 3:
			// 失败时,将file_table中的相应位清空
			memset(&file_table[fd_index], 0, sizeof(struct file));
		case 2:
			sys_free(new_inode);
		case 1:
			// 如果新文件的inode创建失败,
			// 之前位图中分配的inode_no也要恢复
			set_bitmap(&cur_part->inode_btmp, inode_no, 0);
			break;
	}
	sys_free(io_buf);
	return -1;
}

// 打开编号为inode_no的inode对应的文件
// 成功返回文件描述符,否则返回-1
int32_t file_open(uint32_t inode_no, uint8_t flag) {
	int32_t fd_index = get_free_slot();
	if(fd_index == -1) {
		printk("exceed max open files!\n");
		return -1;
	}
	file_table[fd_index].fd_inode = inode_open(cur_part, inode_no);
	// 每次打开文件,要将fd_pos还原为0,即让文件内的指针指向开头
	file_table[fd_index].fd_pos = 0;
	file_table[fd_index].fd_flag = flag;
	bool *write_flag = &file_table[fd_index].fd_inode->write_flag;
	if((flag == FO_WRITEONLY) || (flag == FO_READWRITE)) {
		enum intr_status old_status = get_intr_status();
		disable_intr();
		if(!(*write_flag)) { // 没有进程在写该文件
			*write_flag = true; // 避免多个进程同时写该文件
			set_intr_status(old_status);
		} else { // 写文件失败
			set_intr_status(old_status);
			printk("file is being occupied,try again later!\n");
			return -1;
		}
	}
	return install_task_fd(fd_index);
}

// 关闭文件
int32_t file_close(struct file *file) {
	if(file == NULL) {
		return -1;
	}
	file->fd_inode->write_flag = false;
	inode_close(file->fd_inode);
	file->fd_inode = NULL; // 使文件结构可用
	return 0;
}

// 把buf中的count个字节写入file,
// 成功返回写入的字节数,失败返回-1
int32_t file_write(struct file *file, const void *buf, uint32_t count) {
	if((file->fd_inode->i_size + count) > (BLOCK_SIZE * 140)) {
		// 文件目前最大只支持512 * 140字节
		printk("exceed max file size 71680 byte, write file failed!\n");
		return -1;
	}
	uint8_t *io_buf = sys_malloc(BLOCK_SIZE);
	if(io_buf == NULL) {
		printk("file_write : alloc io_buf memory failed!\n");
		return -1;
	}
	// 文件的所有块的地址
	uint32_t *all_blocks =  (uint32_t*) sys_malloc(BLOCK_SIZE + 48);
	if(all_blocks == NULL) {
		printk("file_write : alloc all_blocks memory failed!\n");
		return -1;
	}
	const uint8_t *src = buf; // src指向buf待写入的数据
	uint32_t written_bytes = 0; // 已写入的数据字节大小
	uint32_t left_bytes = count; // 未写入的数据字节大小
	int32_t block_lba = -1; // 块地址
	uint32_t block_btmp_index = 0; // block在块位图中的索引
	uint32_t sector_index; // 用来索引扇区
	uint32_t sector_lba; // 扇区地址
	uint32_t sector_offset_bytes; // 扇区内字节偏移量
	uint32_t sector_left_bytes; // 扇区内剩余字节量
	uint32_t chunk_size; // 每次写入硬盘的数据块大小
	int32_t indirect_block_table; // 用来获取一级间接表地址
	uint32_t block_index; // 块索引
	// 判断文件是否是第一次写,如果是,先为其分配一个块
	if(file->fd_inode->sectors[0] == 0) {
		block_lba = alloc_block_bitmap(cur_part);
		if(block_lba == -1) {
			printk("file_write : alloc_block_bitmap failed!\n");
			return -1;
		}
		file->fd_inode->sectors[0] = block_lba;
		// 每分配一个块就将位图同步到硬盘
		block_btmp_index = block_lba - cur_part->sp_block->data_lba_start;
		ASSERT(block_btmp_index != 0);
		bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
	}
	// 写入count个字节前,该文件已经占用的块数
	uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
	// 存储count字节后该文件将占用的块数
	uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
	ASSERT(file_will_use_blocks <= 140);
	// 通过此增量判断是否需要分配扇区
	uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;
	// 将写文件所用到的块地址收集到all_blocks,系统中块大小等于扇区大小
	// 后面都统一在all_blocks中获取写入扇区地址
	if(add_blocks == 0) {
		// 在同一扇区内写入数据,不涉及到分配新扇区
		if(file_will_use_blocks <= 12) { // 文件数据量将在12块之内
			block_index = file_has_used_blocks - 1;
			// 指向最后一个已有数据的扇区
			all_blocks[block_index] = file->fd_inode->sectors[block_index];
		} else {
			// 未写入新数据之前已经占用了间接块,需要将间接块地址读进来
			ASSERT(file->fd_inode->sectors[12] != 0);
			indirect_block_table = file->fd_inode->sectors[12];
			ide_read(cur_part->disk, indirect_block_table, all_blocks + 12, 1);
		}
	} else {
		// 若有增量,便涉及到分配新扇区及是否分配一级间接块表
		// 第一种情况 : 12个直接块够用
		if(file_will_use_blocks <= 12) {
			// 先将有剩余空间的可继续用的扇区地址写入all_blocks
			block_index = file_has_used_blocks - 1;
			ASSERT(file->fd_inode->sectors[block_index] != 0);
			all_blocks[block_index] = file->fd_inode->sectors[block_index];
			// 再将未来要用的扇区分配好后写入all_blocks
			block_index = file_has_used_blocks; // 指向第一个要分配的新扇区
			while(block_index < file_will_use_blocks) {
				block_lba = alloc_block_bitmap(cur_part);
				if(block_lba == -1) {
					printk("file_write : alloc_block_bitmap failed!(case 1)\n");
					return -1;
				}
				// 写文件时,不应该存在块未使用但已经分配扇区的情况,
				// 当文件删除时,就会把块地址清0
				ASSERT(file->fd_inode->sectors[block_index] == 0);
				// 确保尚未分配扇区地址
				file->fd_inode->sectors[block_index] = block_lba;
				all_blocks[block_index] = block_lba;
				// 每分配一个块就将位图同步到硬盘
				block_btmp_index = block_lba - cur_part->sp_block->data_lba_start;
				bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
				++block_index; // 下一个分配的新扇区
			}
		} else if(file_has_used_blocks <= 12 && file_will_use_blocks > 12) {
			// 第二种情况 : 旧数据在12个直接块内,新数据将使用间接块
			// 先将有剩余空间的可继续用的扇区地址写入all_blocks
			block_index = file_has_used_blocks - 1;
			// 指向旧数据所在的最后一个扇区
			all_blocks[block_index] = file->fd_inode->sectors[block_index];
			// 创建一级间接块表
			block_lba = alloc_block_bitmap(cur_part);
			if(block_lba == -1) {
				printk("file_write : alloc_block_bitmap failed!(case 2)");
				return -1;
			}
			// 确保一级间接块表未分配
			ASSERT(file->fd_inode->sectors[12] == 0);
			// 分配一级间接块索引表
			indirect_block_table = file->fd_inode->sectors[12] = block_lba;
			block_index = file_has_used_blocks;
			// 第一个未使用的块,即本文件最后一个已经使用的直接块的下一块
			while(block_index < file_will_use_blocks) {
				block_lba = alloc_block_bitmap(cur_part);
				if(block_lba == -1) {
					printk("file_write : alloc_block_bitmap failed!(case 2)");
					return -1;
				}
				if(block_index < 12) {
					// 新创建的0-11块直接存入all_blocks数组
					// 确保尚未分配扇区地址
					ASSERT(file->fd_inode->sectors[block_index] == 0);
					file->fd_inode->sectors[block_index] = block_lba;
					all_blocks[block_index] = block_lba;
				} else {
					// 间接块只写入到all_blocks数组中,待全部分配完成后一次性同步到硬盘
					all_blocks[block_index] = block_lba;
				}
				// 每分配一个块就将位图同步到硬盘
				block_btmp_index = block_lba - cur_part->sp_block->data_lba_start;
				bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
				++block_index; // 下一个新扇区
			}
			// 同步一级间接块表到硬盘
			ide_write(cur_part->disk, indirect_block_table, all_blocks + 12, 1);
		}  else if(file_has_used_blocks > 12) {
			// 第三种情况 : 新数据占据间接块
			// 已经具备了一级间接块表
			ASSERT(file->fd_inode->sectors[12] != 0);
			// 获取一级间接表地址
			indirect_block_table = file->fd_inode->sectors[12];
			// 已使用的间接块已将被读入all_blocks,无需单独收录
			ide_read(cur_part->disk, indirect_block_table, all_blocks + 12, 1);
			// 第一个未使用的间接块,即已使用的间接块的下一块
			block_index = file_has_used_blocks;
			while(block_index < file_will_use_blocks) {
				block_lba = alloc_block_bitmap(cur_part);
				if(block_lba == -1) {
					printk("file_write : alloc_block_bitmap failed!(case 3)");
					return -1;
				}
				all_blocks[block_index++] = block_lba;
				// 每分配一个块就将位图同步到硬盘
				block_btmp_index = block_lba - cur_part->sp_block->data_lba_start;
				bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
			}
			// 同步一级间接块表到硬盘
			ide_write(cur_part->disk, indirect_block_table, all_blocks + 12, 1);
		}
	}
	// 用到的块地址已经收集到all_blocks中,下面开始写数据
	bool first_write_block = true; // 含有剩余空间的块标识
	// 置fd_pos为文件大小-1,下面在写数据时随时更新
	file->fd_pos = file->fd_inode->i_size - 1;
	while(written_bytes < count) {
		memset(io_buf, 0, BLOCK_SIZE);
		sector_index = file->fd_inode->i_size / BLOCK_SIZE;
		sector_lba = all_blocks[sector_index];
		sector_offset_bytes = file->fd_inode->i_size % BLOCK_SIZE;
		sector_left_bytes = BLOCK_SIZE - sector_offset_bytes;
		// 判断此次写入硬盘的数据大小
		chunk_size = left_bytes < sector_left_bytes ? left_bytes : sector_left_bytes;
		if(first_write_block) {
			ide_read(cur_part->disk, sector_lba, io_buf, 1);
			first_write_block =false;
		}
		memcpy(io_buf + sector_offset_bytes, src, chunk_size);
		ide_write(cur_part->disk, sector_lba, io_buf, 1);
		printk("file write at lba %x\n", sector_lba); // 调试
		src += chunk_size; // 将指针移到下一个新数据
		file->fd_inode->i_size += chunk_size; // 更新文件大小
		file->fd_pos += chunk_size;
		written_bytes += chunk_size;
		left_bytes -= chunk_size;
	}
	inode_sync(cur_part, file->fd_inode, io_buf);
	sys_free(all_blocks);
	sys_free(io_buf);
	return written_bytes;
}

// 从文件file中读取count个字节写入buf,
// 成功返回读出的字节数,若到文件尾则返回-1
int32_t file_read(struct file *file, void *buf, uint32_t count) {
	uint8_t *buf_dst = (uint8_t*) buf;
	uint32_t size = count;
	uint32_t size_left = count;
	// 若要读取的字节数超过了文件可读的剩余量,
	// 就用剩余量作为待读取的字节数
	if((file->fd_pos + count) > file->fd_inode->i_size) {
		size = file->fd_inode->i_size - file->fd_pos;
		size_left = size;
		if(size == 0) { // 若到文件尾,则返回-1
			return -1;
		}
	}
	uint8_t *io_buf = (uint8_t*) sys_malloc(BLOCK_SIZE);
	if(io_buf == NULL) {
		printk("file_read : alloc memory failed!\n");
		return -1;
	}
	uint32_t *all_blocks = (uint32_t*) sys_malloc(BLOCK_SIZE + 48);
	// 用来记录文件所有的块地址
	if(all_blocks == NULL) {
		printk("file_read : alloc_memory failed!\n");
		return -1;
	}
	// 数据所在块的起始地址
	uint32_t block_read_start_index = file->fd_pos / BLOCK_SIZE;
	// 数据所在块的结束地址
	uint32_t block_read_end_index = (file->fd_pos + size) / BLOCK_SIZE;
	ASSERT(block_read_start_index < 139 && block_read_end_index < 139);
	// 如增量为0,表示数据在同一扇区
	uint32_t read_blocks = block_read_start_index - block_read_end_index;
	int32_t indirect_block_table; // 用来获取一级间接表地址
	uint32_t block_index; // 获取待读的块地址
	// 以下开始构建all_blocks块地址数组,专门存储用到的块地址
	if(read_blocks == 0) { // 不跨扇区
		if(block_read_end_index < 12) { // 待读数据在12个直接块之内
			block_index = block_read_end_index;
			all_blocks[block_index] = file->fd_inode->sectors[block_index];
		} else { // 若用到一级间接块表,需要将表间接块读进来
			indirect_block_table = file->fd_inode->sectors[12];
			ide_read(cur_part->disk, indirect_block_table, all_blocks + 12, 1);
		}
	} else { // 跨扇区
		// 第一种情况 : 起始块和结束块属于直接块
		if(block_read_end_index < 12) { // 数据结束所在的块属于直接块
			block_index = block_read_start_index;
			while(block_index <= block_read_end_index) {
				all_blocks[block_index] = file->fd_inode->sectors[block_index];
				++block_index;
			}
		} else if(block_read_start_index < 12 && block_read_end_index >= 12) {
			// 第二种情况 : 待读入的数据跨越直接块和间接块两类
			// 先将直接块地址写入all_blocks
			block_index = block_read_start_index;
			while(block_index < 12) {
				all_blocks[block_index] = file->fd_inode->sectors[block_index];
				++block_index;
			}
			// 确保已经分配了一级间接块表
			ASSERT(file->fd_inode->sectors[12] != 0);
			// 再将间接块地址写入all_blocks
			indirect_block_table = file->fd_inode->sectors[12];
			// 将一级间接块表读进来写入到第13块个块的位置之后
			ide_read(cur_part->disk, indirect_block_table, all_blocks + 12, 1);
		} else {
			// 第三种情况 : 数据在间接块中
			// 确保已经分配了一级间接块表
			ASSERT(file->fd_inode->sectors[12] != 0);
			// 获取一级间接表地址
			indirect_block_table = file->fd_inode->sectors[12];
			// 将一级间接块表读进来写入到第13块个块的位置之后
			ide_read(cur_part->disk, indirect_block_table, all_blocks + 12, 1);
		}
	}
	// 用到的块地址已经收集到all_blocks中,下面开始读数据
	uint32_t sector_index;
	uint32_t sector_lba;
	uint32_t sector_offset_bytes;
	uint32_t sector_left_bytes;
	uint32_t chunk_size;
	uint32_t bytes_read = 0;
	while(bytes_read < size) {
		sector_index = file->fd_pos / BLOCK_SIZE;
		sector_lba = all_blocks[sector_index];
		sector_offset_bytes = file->fd_pos % BLOCK_SIZE;
		sector_left_bytes = BLOCK_SIZE - sector_offset_bytes;
		// 待读入的数据大小
		chunk_size = size_left < sector_left_bytes ? size_left : sector_left_bytes;
		memset(io_buf, 0, BLOCK_SIZE); // 不清空也可以
		ide_read(cur_part->disk, sector_lba, io_buf, 1);
		memcpy(buf_dst, io_buf + sector_offset_bytes, chunk_size);
		buf_dst += chunk_size;
		file->fd_pos += chunk_size;
		bytes_read += chunk_size;
		size_left -= chunk_size;
	}
	sys_free(all_blocks);
	sys_free(io_buf);
	return bytes_read;
}















































































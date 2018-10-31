#include "types.h"
#include "string.h"
#include "ide.h"
#include "debug.h"
#include "inode.h"
#include "list.h"
#include "global.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"
#include "fs.h"

// inode位置结构体
struct inode_position {
	bool cross_sector; // inode是否跨扇区
	uint32_t sector_lba; // inode所在的扇区号
	uint32_t sector_offset; // inode在扇区内的字节偏移量
};

// 获取inode所在的扇区和扇区内的偏移量
static void inode_locate(struct partition *part, uint32_t inode_no, \
	struct inode_position *inode_pos) {
	// inode_table在硬盘上是连续的
	ASSERT(inode_no < 4096);
	uint32_t inode_table_lba = part->sp_block->inode_table_lba;
	uint32_t inode_size = sizeof(struct inode);
	// 第inode_no号inode相对于inode_table_lba的字节偏移量
	uint32_t offset_size = inode_no * inode_size;
	// 第inode_no号inode相对于inode_table_lba的扇区偏移量
	uint32_t offset_sector = offset_size / 512;
	// 待查找的inode所在扇区中的起始地址
	uint32_t offset_in_sector = offset_size % 512;
	// 判断此inode是否跨越2个扇区
	uint32_t left_in_sector = 512 - offset_in_sector;
	// 若扇区中剩余的空间不足以容纳一个inode,则必然跨越2个扇区
	if(left_in_sector < inode_size) {
		inode_pos->cross_sector = true;
	} else {
		inode_pos->cross_sector = false;
	}
	inode_pos->sector_lba = inode_table_lba + offset_sector;
	inode_pos->sector_offset = offset_in_sector;
}

// 将inode写入到分区part
void inode_sync(struct partition *part, struct inode *inode, void *io_buf) {
	// io_buf是用于硬盘io的缓冲区
	uint8_t inode_no = inode->i_no;
	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);
	ASSERT(inode_pos.sector_lba <= (part->lba_start + part->sector_count));
	// 硬盘中inode的成员inode_tag和open_count是不需要的,
	// 它们只在内存中记录链表位置和被多少进程共享
	struct inode pure_inode;
	memcpy(&pure_inode, inode, sizeof(struct inode));
	// 将inode同步到硬盘
	pure_inode.open_count = 0;
	pure_inode.write_flag = false;
	pure_inode.inode_tag.prev = NULL;
	pure_inode.inode_tag.next = NULL;
	char *inode_buf = (char*) io_buf;
	if(inode_pos.cross_sector) {
		// 跨扇区,需要读出两个扇区再写入两个扇区
		// 读写硬盘是以扇区为单位,若写入的数据小于1扇区,
		// 要将原硬盘上的内容先读出来再和新数据拼成1扇区后再写入
		ide_read(part->disk, inode_pos.sector_lba, inode_buf, 2);
		memcpy(inode_buf + inode_pos.sector_offset, &pure_inode, sizeof(struct inode));
		ide_write(part->disk, inode_pos.sector_lba, inode_buf, 2);
	} else { // 不跨扇区
		ide_read(part->disk, inode_pos.sector_lba, inode_buf, 1);
		memcpy(inode_buf + inode_pos.sector_offset, &pure_inode, sizeof(struct inode));
		ide_write(part->disk, inode_pos.sector_lba, inode_buf, 1);
	}
}

// 根据inode_no返回相应的inode
struct inode *inode_open(struct partition *part, uint32_t inode_no) {
	// 先在已打开的inode链表中找inode,此链表是为提速创建的缓冲区
	struct list_ele *ele = part->open_inodes.head.next;
	struct inode *inode_found;
	while(ele != &part->open_inodes.tail) {
		inode_found = ELE2ENTRY(struct inode, inode_tag, ele);
		if(inode_found->i_no == inode_no) {
			++inode_found->open_count;
			return inode_found;
		}
		ele = ele->next;
	}
	// 由于open_inodes链表中找不到,
	// 下面从硬盘上读入此inode并加入到此链表
	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);
	// 为使通过sys_malloc创建的新inode被所有任务共享,
	// 需要将inode置于内核空间,故需要临时将cur_thread->pgdir置为NULL
	struct task_struct *cur_thread = current_thread();
	uint32_t *pgdir_bak = cur_thread->pgdir;
	cur_thread->pgdir = NULL;
	// 以上三行代码完成后,下面分配的内存将位于内核区
	inode_found = (struct inode*) sys_malloc(sizeof(struct inode));
	cur_thread->pgdir = pgdir_bak; // 恢复pgdir
	char *inode_buf;
	if(inode_pos.cross_sector) { // 跨扇区
		inode_buf = (char*) sys_malloc(1024);
		// inode表是被partition_format函数连续写入扇区的
		ide_read(part->disk, inode_pos.sector_lba, inode_buf, 2);
	} else { // 未跨扇区
		inode_buf = (char*) sys_malloc(512);
		ide_read(part->disk, inode_pos.sector_lba, inode_buf, 1);
	}
	memcpy(inode_found, inode_buf + inode_pos.sector_offset, sizeof(struct inode));
	// 因为要用到此inode,故将其插入到队首便于提前检索到
	list_push(&part->open_inodes, &inode_found->inode_tag);
	inode_found->open_count = 1;
	sys_free(inode_buf);
	return inode_found;
}

// 关闭inode或减少inode的打开数
void inode_close(struct inode *inode) {
	// 若没有进程再打开此文件,将此inode去掉并释放空间
	enum intr_status old_status = get_intr_status();
	disable_intr();
	if(--inode->open_count == 0) {
		// 将inode从part->open_inodes中去掉
		list_remove(&inode->inode_tag);
		// 释放inode时要保证释放的是内核内存池
		struct task_struct *cur_thread = current_thread();
		uint32_t *pgdir_bak = cur_thread->pgdir;
		cur_thread->pgdir = NULL;
		sys_free(inode);
		cur_thread->pgdir = pgdir_bak; // 恢复pgdir
	}
	set_intr_status(old_status);
}

// 将硬盘分区part上的inode清空
void inode_delete(struct partitin *part, uint32_t inode_no, void *buf) {
	ASSERT(inode_no < 4096);
	struct inode_position inode_pos;
	inode_locate(part, inode_no, &inode_pos);
	ASSERT(inode_pos.sector_lba <= (part->lba_start + part->sector_count));
	char *inode_buf = (char*) io_buf;
	if(inode_pos.cross_sector) { // 跨扇区
		// 将原硬盘上的内容先读出来
		ide_read(part->disk, inode_pos.sector_lba, inode_buf, 2);
		// 将inode_buf清0
		memset(inode_buf + inode_pos.sector_offset, 0, sizeof(struct inode));
		// 用清0的内存数据覆盖磁盘
		ide_write(part->disk, inode_pos.sector_lba, inode_buf, 2);
	} else { // 不跨扇区
		// 将原硬盘上的内容先读出来
		ide_read(part->disk, inode_pos.sector_lba, inode_buf, 1);
		// 将inode_buf清0
		memset(inode_buf + inode_pos.sector_offset, 0, sizeof(struct inode));
		// 用清0的内存数据覆盖磁盘
		ide_write(part->disk, inode_pos.sector_lba, inode_buf, 1);
	}
}

// 回收inode的数据块和inode本身
void inode_release(struct partition *part, uint32_t inode_no) {
	struct inode *inode_del = inode_open(part, inode_no);
	ASSERT(inode_del->i_no == inode_no);
	// 1 回收inode占用的所有块
	uint8_t block_index = 0;
	uint8_t block_count = 12;
	uint32_t block_btmp_index;
	uint32_t all_blocks[140] = {0}; // 12个直接块 + 128个间接块
	// a 先将前12个直接块存入all_blocks
	while(block_index < 12) {
		all_blocks[block_index] = inode_del->sectors[block_index];
		++block_index;
	}
	// b 如果一级间接块表存在,将其128个间接块读到all_blocks[12-],
	// 然后释放一级间接块表所占的扇区
	if(inode_del->sectors[12] != 0) {
		ide_read(part->disk, inode_del->sectors[12], all_blocks + 12, 1);
		block_count = 140;
		// 回收一级间接块表占用的扇区
		block_btmp_index = inode_del->sectors[12] - part->sp_block->data_lba_start;
		ASSERT(block_btmp_index > 0);
		set_bitmap(&part->block_btmp, block_btmp_index, 0);
		bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
	}
	// c inode所有的块地址已经收集到all_blocks中,下面逐个回收
	block_index = 0;
	while(block_index < block_count) {
		if(all_blocks[block_index] != 0) {
			block_btmp_index = 0;
			block_btmp_index = all_blocks[block_index] - part->sp_block->data_lba_start;
			ASSERT(block_btmp_index > 0);
			set_bitmap(&part->block_btmp, block_btmp_index, 0);
			bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
		}
		++block_index;
	}
	// 2 回收该inode所占用的inode
	set_bitmap(&part->inode_btmp, inode_no, 0);
	bitmap_sync(cur_part, inode_no, INODE_BITMAP);
	
	// 以下inode_delete是调试用的
	// 此函数会在inode_table中将此inode清0
	// 但实际上是不需要的,inode分配是由inode位图控制的
	// 硬盘上的数据不需要清0,可以直接覆盖
	void *io_buf = sys_malloc(1024);
	inode_delete(part, inode_no, io_buf);
	sys_free(io_buf);
	
	inode_close(inode_del);
}

// 初始化inode
void inode_init(uint32_t inode_no, struct inode *inode) {
	inode->i_no = inode_no;
	inode->i_size = 0;
	inode->open_count = 0;
	inode->write_flag = false;
	// 初始化块索引数组sectors
	for(uint8_t i = 0; i < 13; i++) {
		inode->sectors[i] = 0;
	}
}


















































































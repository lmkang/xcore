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


















































































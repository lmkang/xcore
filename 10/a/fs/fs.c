#include "ide.h"
#include "global.h"
#include "fs.h"
#include "superblock.h"
#include "directory.h"
#include "console.h"
#include "string.h"
#include "syscall.h"
#include "debug.h"
#include "inode.h"

// 格式化分区,也就是初始化分区的元信息,创建文件系统
static void partition_format(struct partition *part) {
	// blocks_btmp_init(为方便实现,一个块大小是一扇区)
	uint32_t boot_sector_secs = 1;
	uint32_t super_block_secs = 1;
	// inode位图占用的扇区数,最多支持4096个文件
	uint32_t inode_btmp_secs = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
	uint32_t inode_table_secs = DIV_ROUND_UP((sizeof(struct inode) * MAX_FILES_PER_PART), SECTOR_SIZE);
	uint32_t used_secs = boot_sector_secs + super_block_secs + inode_btmp_secs + inode_table_secs;
	uint32_t free_secs = part->sector_count - used_secs;
	// 简单处理块位图占据的扇区数
	uint32_t block_btmp_secs;
	block_btmp_secs = DIV_ROUND_UP(free_secs, BITS_PER_SECTOR);
	// block_btmp_bit_len是位图中位的长度,也是可用块的数量
	uint32_t block_btmp_bit_len = free_secs - block_btmp_secs;
	block_btmp_secs = DIV_ROUND_UP(block_btmp_bit_len, BITS_PER_SECTOR);
	// 超级块初始化
	struct super_block sp_block;
	sp_block.magic = 0x19940625;
	sp_block.sector_count = part->sector_count;
	sp_block.inode_count = MAX_FILES_PER_PART;
	sp_block.part_lba_base = part->start_lba;
	sp_block.block_btmp_lba = sp_block.part_lba_base + 2;
	// 第0块是引导块,第1块是超级块
	sp_block.block_btmp_secs = block_btmp_secs;
	sp_block.inode_btmp_lba = sp_block.block_btmp_lba + sp_block.block_btmp_secs;
	sp_block.inode_btmp_secs = inode_btmp_secs;
	sp_block.inode_table_lba = sp_block.inode_btmp_lba + sp_block.inode_btmp_secs;
	sp_block.inode_table_secs = inode_table_secs;
	sp_block.data_start_lba = sp_block.inode_table_lba + sp_block.inode_table_secs;
	sp_block.root_inode_no = 0;
	sp_block.dir_entry_size = sizeof(struct dir_entry);
	
	printk("%s info: \n", part->name);
	printk(" magic: 0x%x\n part_lba_base: 0x%x\n all_sectors: 0x%x\n inode_count: 0x%x\n \
		bloc_btmp_lba: 0x%x\n block_btmp_sectors: 0x%x\n inode_btmp_lba: 0x%x\n \
		inode_btmp_sectors :0x%x\n inode_table_lba: 0x%x\n inode_table_sectors: 0x%x\n \
		data_start_lba: 0x%x\n", sp_block.magic, sp_block.part_lba_base, sp_block.sector_count, \
		sp_block.inode_count, sp_block.block_btmp_lba, sp_block.block_btmp_secs, sp_block.inode_btmp_lba, \
		sp_block.inode_btmp_secs, sp_block.inode_table_lba, sp_block.inode_table_secs, sp_block.data_start_lba);
	
	struct disk *disk = part->disk;
	// 1 将超级块写入本分区的1扇区
	ide_write(disk, part->start_lba + 1, &sp_block, 1);
	printk(" super_block_lba: 0x%x\n", part->start_lba + 1);
	// 找出数据量最大的元信息,用其尺寸做存储缓冲区
	uint32_t buf_size = \
		(sp_block.block_btmp_secs >= sp_block.inode_btmp_secs ? sp_block.block_btmp_secs : sp_block.inode_btmp_secs);
	buf_size = (buf_size >= sp_block.inode_table_secs ? buf_size : sp_block.inode_table_secs) * SECTOR_SIZE;
	uint8_t *buf = (uint8_t *) sys_malloc(buf_size);
	// 2 将块位图初始化并写入sp_block.block_btmp_lba
	// 初始化块位图block_bitmap
	buf[0] |= 0x01; // 第0个块预留给根目录,位图中先占位
	uint32_t block_btmp_last_byte = block_btmp_bit_len / 8;
	uint8_t block_btmp_last_bit = block_btmp_bit_len % 8;
	// last_size是位图中所在最后一个扇区中,不足1扇区的其余部分
	uint32_t last_size = SECTOR_SIZE - (block_btmp_last_byte % SECTOR_SIZE);
	// 1 先将位图最后1字节到其所在的扇区的结束全置为1,
	// 即超出实际块数的部分直接置为已占用
	memset(&buf[block_btmp_last_byte], 0xff, last_size);
	// 2 再将上一步中覆盖的最后1字节内的有效位重新置0
	uint8_t bit_idx = 0;
	while(bit_idx <= block_btmp_last_bit) {
		buf[block_btmp_last_byte] &= ~(1 << bit_idx++);
	}
	ide_write(disk, sp_block.block_btmp_lba, buf, sp_block.block_btmp_secs);
	
	// 3 将inode位图初始化并写入sp_block.inode_btmp_lba
	// 先清空缓冲区
	memset(buf, 0, buf_size);
	buf[0] |= 0x01; // 第0个inode分给了根目录
	// 由于inode_table中共4096个inode,
	// 位图inode_bitmap正好占用1扇区,即inode_btmp_secs=1,
	// 所以位图中的位全都代表inode_table中的inode,
	// 无需再像block_bitmap那样单独处理最后1扇区的剩余部分,
	// inode_bitmap所在的扇区中没有多余的无效位
	ide_write(disk, sp_block.inode_btmp_lba, buf, sp_block.inode_btmp_secs);
	// 4 将inode数组初始化并写入sp_block.inode_table_lba
	// 准备写inode_table中的第0项,即根目录所在的inode
	memset(buf, 0, buf_size); // 先清空缓冲区buf
	struct inode *node = (struct inode*) buf;
	node->i_size = sp_block.dir_entry_size * 2; // .和..
	node->i_no = 0; // 根目录占inode数组中第0个inode
	node->i_sectors[0] = sp_block.data_start_lba;
	ide_write(disk, sp_block.inode_table_lba, buf, sp_block.inode_table_secs);
	// 5 将根目录写入sp_block.data_start_lba
	// 写入根目录的两个目录项.和..
	memset(buf, 0, buf_size);
	struct dir_entry *dir_ent = (struct dir_entry*) buf;
	// 初始化当前目录
	memcpy(dir_ent->filename, ".", 1);
	dir_ent->i_no = 0;
	dir_ent->f_type = FT_DIRECTORY;
	++dir_ent;
	// 初始化当前目录父目录, ".."
	memcpy(dir_ent->filename, "..", 2);
	dir_ent->i_no = 0; // 根目录的父目录依然是根目录自己
	dir_ent->f_type = FT_DIRECTORY;
	// sp_block.data_start_lba已经分配给了根目录,里面是根目录的目录项
	ide_write(disk, sp_block.data_start_lba, buf, 1);
	
	printk(" root_dir_lba: 0x%x\n", sp_block.data_start_lba);
	printk("%s format done\n", part->name);
	sys_free(buf);
}

// 在磁盘上搜索文件系统,若没有则格式化分区创建文件系统
void fs_init() {
	uint8_t channel_no = 0;
	uint8_t dev_no;
	uint8_t part_idx = 0;
	struct super_block *sp_block = (struct super_block*) sys_malloc(SECTOR_SIZE);
	if(sp_block == NULL) {
		PANIC("alloc memory failed!");
	}
	printk("searching  filesystem......\n");
	while(channel_no < channel_count) {
		dev_no = 0;
		while(dev_no < 2) { // 跨过裸盘
			if(dev_no == 0) {
				++dev_no;
				continue;
			}
			struct disk *disk = &channels[channel_no].devices[dev_no];
			struct partition *part = disk->primary_parts;
			while(part_idx < 12) { // 4个主分区 + 8个逻辑
				if(part_idx == 4) { // 开始处理逻辑分区
					part = disk->logic_parts;
				}
				// channels数组是全局变量,默认值为0,disk属于其嵌套结构
				// partition又是disk的嵌套结构,因此partition中的成员默认也为0
				// 若partition未初始化,则partition中的成员仍为0
				if(part->sector_count != 0) { // 分区存在
					memset(sp_block, 0, SECTOR_SIZE);
					// 读出分区的超级块,根据魔数是否正确来判断是否存在文件系统
					ide_read(disk, part->start_lba + 1, sp_block, 1);
					// 只支持自己的文件系统,若磁盘上已经有文件系统就不再格式化了
					if(sp_block->magic == 0x19940625) {
						printk("%s has filesystem\n", part->name);
					} else { // 其他文件系统不支持,一律按照无文件系统处理
						printk("formating %s's partition %s......\n", disk->name, part->name);
						partition_format(part);
					}
				}
				++part_idx;
				++part; // 下一分区
			}
			++dev_no; // 下一磁盘
		}
		++channel_no; // 下一通道
	}
	sys_free(sp_block);
}
#include "fs.h"
#include "global.h"
#include "print.h"
#include "ide.h"
#include "memory.h"
#include "string.h"
#include "debug.h"

// 按硬盘数计算的通道数
extern uint8_t channel_count;

// 有两个ide通道
extern struct ide_channel channels[2];

// 分区队列
extern struct list partition_list;

// 默认情况下操作的分区
struct partition *cur_part;

// 分区挂载,在分区链表中找到名为part_name的分区,并将其指针赋值给cur_part
static bool partition_mount(struct list_ele *ele, int arg) {
	char *part_name = (char*) arg;
	struct partition *part = ELE2ENTRY(struct partition, part_tag, ele);
	if(!strcmp(part->name, part_name)) {
		cur_part = part;
		struct disk *disk = cur_part->disk;
		struct super_block *sp_block = (struct super_block*) sys_malloc(SECTOR_SIZE);
		// 在内存中创建分区cur_part的超级块
		cur_part->sp_block = (struct super_block*) sys_malloc(sizeof(struct super_block));
		if(cur_part->sp_block == NULL) {
			PANIC("alloc memory failed!");
		}
		// 读入超级块
		memset(sp_block, 0, SECTOR_SIZE);
		ide_read(disk, cur_part->lba_start + 1, sp_block, 1);
		// 把sp_block复制到分区的超级块中
		memcpy(cur_part->sp_block, sp_block, sizeof(struct super_block));
		// 将硬盘上的块位图读入到内存
		cur_part->block_btmp.bits = (uint8_t*) \
			sys_malloc(sp_block->block_btmp_secs * SECTOR_SIZE);
		if(cur_part->block_btmp.bits == NULL) {
			PANIC("alloc memory failed!");
		}
		cur_part->block_btmp.byte_len = sp_block->block_btmp_secs * SECTOR_SIZE;
		// 从硬盘上读入块位图到分区的block_btmp.bits
		ide_read(disk, sp_block->block_btmp_lba, cur_part->block_btmp.bits, \
			sp_block->block_btmp_secs);
		// 将硬盘上的inode位图读入到内存
		cur_part->inode_btmp.bits = (uint8_t*) \
			sys_malloc(sp_block->inode_btmp_secs * SECTOR_SIZE);
		if(cur_part->inode_btmp.bits == NULL) {
			PANIC("alloc memory failed!");
		}
		cur_part->inode_btmp.byte_len = sp_block->inode_btmp_secs * SECTOR_SIZE;
		// 从硬盘上读入inode位图到分区的inode_btmp.bits
		ide_read(disk, sp_block->inode_btmp_lba, cur_part->inode_btmp.bits, \
			sp_block->inode_btmp_secs);
		list_init(&cur_part->open_inodes);
		
		printk("mount %s done!\n", part->name);
		return true; // 返回true,停止遍历
	}
	return false; // 返回false,继续遍历
}

// 格式化分区
static void partition_format(struct partition *part) {
	// 为方便实现,一块大小就是一扇区
	uint32_t boot_sector_secs = 1;
	uint32_t super_block_secs = 1;
	uint32_t inode_btmp_secs = DIV_ROUND_UP(MAX_FILE_COUNT, SECTOR_BIT_COUNT);
	uint32_t inode_table_secs = DIV_ROUND_UP(sizeof(struct inode) * MAX_FILE_COUNT, SECTOR_SIZE);
	uint32_t used_secs = boot_sector_secs + super_block_secs + \
		inode_btmp_secs + inode_table_secs;
	uint32_t free_secs = part->sector_count - used_secs;
	// 简单处理块位图占据的扇区数
	uint32_t block_btmp_secs;
	block_btmp_secs = DIV_ROUND_UP(free_secs, SECTOR_BIT_COUNT);
	// block_btmp_bit_len是位图中位的长度,也是可用块的数量
	uint32_t block_btmp_bit_len = free_secs - block_btmp_secs;
	block_btmp_secs = DIV_ROUND_UP(block_btmp_bit_len, SECTOR_BIT_COUNT);
	// 超级块初始化
	struct super_block sp_block;
	sp_block.magic = 0x19940625;
	sp_block.sector_count = part->sector_count;
	sp_block.inode_count = MAX_FILE_COUNT;
	sp_block.part_lba_start = part->lba_start;
	sp_block.block_btmp_lba = sp_block.part_lba_start + 2;
	// 第0块是引导块,第1块是超级块
	sp_block.block_btmp_secs = block_btmp_secs;
	sp_block.inode_btmp_lba = sp_block.block_btmp_lba + sp_block.block_btmp_secs;
	sp_block.inode_btmp_secs = inode_btmp_secs;
	sp_block.inode_table_lba = sp_block.inode_btmp_lba + sp_block.inode_btmp_secs;
	sp_block.inode_table_secs = inode_table_secs;
	sp_block.data_lba_start = sp_block.inode_table_lba + sp_block.inode_table_secs;
	sp_block.root_inode_no = 0;
	sp_block.dir_entry_size = sizeof(struct dir_entry);
	
	printk("%s info : \n", part->name);
	printk(" magic : %x\n part_lba_start : %x\n all_sectors : %x\n inode_count : %x\n "
		"block_btmp_lba : %x\n block_btmp_sectors : %x\n inode_btmp_lba : %x\n "
		"inode_btmp_sectors : %x\n inode_table_lba : %x\n inode_table_sectors : %x\n "
		"data_lba_start : %x\n", sp_block.magic, sp_block.part_lba_start, sp_block.sector_count, \
		sp_block.inode_count, sp_block.block_btmp_lba, sp_block.block_btmp_secs, sp_block.inode_btmp_lba, \
		sp_block.inode_btmp_secs, sp_block.inode_table_lba, sp_block.inode_table_secs, sp_block.data_lba_start);
	
	struct disk *disk = part->disk;
	// 1 将超级块写入本分区的1扇区
	ide_write(disk, part->lba_start + 1, &sp_block, 1);
	printk("super_block_lba : %x\n", part->lba_start + 1);
	// 找出数据量最大的元信息,用其尺寸做存储缓冲区
	uint32_t buf_size = (sp_block.block_btmp_secs >= sp_block.inode_btmp_secs ? \
		sp_block.block_btmp_secs : sp_block.inode_btmp_secs);
	buf_size = (buf_size >= sp_block.inode_table_secs ? buf_size : \
		sp_block.inode_table_secs) * SECTOR_SIZE;
	uint8_t *buf = (uint8_t*) sys_malloc(buf_size);
	// 2 将块位图初始化并写入sp_block.block_btmp_lba
	buf[0] |= 0x01; // 第0个块预留给根目录,位图中先占位
	uint32_t block_btmp_last_byte = block_btmp_bit_len / 8;
	uint8_t block_btmp_last_bit = block_btmp_bit_len % 8;
	// last_siz是位图所在最后一个扇区中,不足1扇区的其余部分
	uint32_t last_size = SECTOR_SIZE - (block_btmp_last_byte % SECTOR_SIZE);
	// 1 先将位图最后1字节到其所在的扇区的结束全置为1,
	// 超出实际块数的部分直接置为已占用
	memset(&buf[block_btmp_last_byte], 0xff, last_size);
	// 2 再将上一步中覆盖的最后1字节内的有效位重新置0
	for(uint8_t i = 0; i <= block_btmp_last_bit; i++) {
		buf[block_btmp_last_byte] &= ~(1 << i);
	}
	ide_write(disk, sp_block.block_btmp_lba, buf, sp_block.block_btmp_secs);
	// 3 将inode位图初始化并写入sp_block.inode_btmp_lba
	// 先清空缓冲区
	memset(buf, 0, buf_size);
	buf[0] |= 0x01; // 第0个inode分给了根目录
	// 由于inode_table中共4096个inode,
	// 位图inode_btmp正好占用1扇区,
	// 即inode_btmp_secs等于1,
	// 所以位图中的位全都代表inode_table中的inode
	// 无需再像block_btmp那样单独处理最后1扇区的剩余部分
	// inode_btmp所在的扇区中没有多余的无效位
	ide_write(disk, sp_block.inode_btmp_lba, buf, sp_block.inode_btmp_secs);
	// 4 将inode数组初始化并写入sp_block.inode_table_lba
	// 准备写inode_table中的第0项,即根目录所在的inode
	memset(buf, 0, buf_size);
	struct inode *inode = (struct inode*) buf;
	inode->i_size = sp_block.dir_entry_size * 2; // .和..
	inode->i_no = 0; // 根目录占inode数组中第0个inode
	inode->sectors[0] = sp_block.data_lba_start;
	ide_write(disk, sp_block.inode_table_lba, buf, sp_block.inode_table_secs);
	// 5 将根目录写入sp_block.data_lba_start
	// 写入根目录的两个目录项.和..
	memset(buf, 0, buf_size);
	struct dir_entry *dir_ent = (struct dir_entry*) buf;
	// 初始化当前目录"."
	memcpy(dir_ent->filename, ".", 1);
	dir_ent->i_no = 0;
	dir_ent->f_type = FT_DIRECTORY;
	++dir_ent;
	// 初始化当前目录父目录".."
	memcpy(dir_ent->filename, "..", 2);
	dir_ent->i_no = 0; // 根目录的父目录依然是根目录自己
	dir_ent->f_type = FT_DIRECTORY;
	// sp_block.data_lba_start已经分配给了根目录,里面是根目录的目录项
	ide_write(disk, sp_block.data_lba_start, buf, 1);
	
	printk("root_dir_lba : %x\n", sp_block.data_lba_start);
	printk("%s format done!\n", part->name);
	
	sys_free(buf);
}

// 在磁盘上搜索文件系统,若没有则格式化分区,创建文件系统
void fs_init(void) {
	uint8_t channel_no = 0;
	uint8_t dev_no;
	uint8_t part_index = 0;
	// sp_block用来存储从硬盘上读入的超级块
	struct super_block *sp_block = (struct super_block*) sys_malloc(SECTOR_SIZE);
	if(sp_block == NULL) {
		PANIC("alloc memory failed!");
	}
	printk("searching filesystem......\n");
	while(channel_no < channel_count) {
		dev_no = 0;
		while(dev_no < 1) { // 目前只有一块硬盘100MB
			struct disk *disk = &channels[channel_no].devices[dev_no];
			struct partition *part = disk->primary_parts;
			while(part_index < 12) { // 4个主分区 + 8个逻辑分区
				if(part_index == 4) { // 开始处理逻辑分区
					part = disk->logic_parts;
				}
				// channels数组是全局变量,默认值为0,disk属于其嵌套结构
				// partition又是disk的嵌套结构,因此partition中的成员默认也是0
				// 若partition未初始化,则partition中的成员仍为0
				if(part->sector_count != 0) { // 分区存在
					memset(sp_block, 0, SECTOR_SIZE);
					// 读取分区超级块的魔数,判断是否存在文件系统
					// 只支持自己的文件系统,若已存在则不再格式化
					if(sp_block->magic == 0x19940625) {
						printk("%s has filesystem\n", part->name);
					} else { // 不支持其他文件系统,一律按无文件系统处理
						printk("unknown filesystem, formatting %s partition %s......\n", \
							disk->name, part->name);
						partition_format(part);
					}
				}
				++part_index;
				++part;
			}
			++dev_no;
		}
		++channel_no;
	}
	sys_free(sp_block);
	// 确定默认操作的分区
	char default_part[] = "sda1";
	// 挂载分区
	list_traversal(&partition_list, partition_mount, (int) default_part);
}

























































































#include "fs.h"
#include "global.h"
#include "print.h"
#include "ide.h"
#include "memory.h"
#include "string.h"
#include "debug.h"
#include "directory.h"
#include "file.h"

// 按硬盘数计算的通道数
extern uint8_t channel_count;

// 有两个ide通道
extern struct ide_channel channels[2];

// 分区队列
extern struct list partition_list;

// 根目录
extern struct directory root_dir;

// 文件表
extern struct file file_table[MAX_FILE_OPEN];

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

// 将最上层路径名称解析出来
static char *parse_path(char *pathname, char *name_store) {
	if(pathname[0] == '/') { // 根目录不需要单独解析
		// 路径中出现1个或多个连续的字符'/',将这些'/'跳过
		while(*(++pathname) == '/');
	}
	// 开始一般的路径解析
	while(*pathname != '/' && *pathname != 0) {
		*name_store++ = *pathname++;
	}
	if(pathname[0] == 0) { // 路径字符串为空,返回NULL
		return NULL;
	}
	return pathname;
}

// 返回路径深度,比如/a/b/c,深度为3
int32_t get_path_depth(char *pathname) {
	ASSERT(pathname != NULL);
	char *p = pathname;
	char name[MAX_FILENAME_LEN];
	uint32_t depth = 0;
	// 解析路径,从中拆分出各级名称
	p = parse_path(p, name);
	while(name[0]) {
		++depth;
		memset(name, 0, MAX_FILENAME_LEN);
		if(p) {
			p = parse_path(p, name);
		}
	}
	return depth;
}

// 搜索文件pathname,若找到则返回其inode号,否则返回-1
static int32_t search_file(const char *pathname, struct path_record *path_record) {
	// 若待查找的是根目录,为避免下面无用的查找,直接返回已知根目录信息
	if(!strcmp(pathname, "/") || !strcmp(pathname, "/.") || \
		!strcmp(pathname, "/..")) {
		path_record->parent_dir = &root_dir;
		path_record->f_type = FT_DIRECTORY;
		path_record->searched_path[0] = 0; // 搜索路径置空
		return 0;
	}
	uint32_t path_len = strlen(pathname);
	// 保证pathname至少是这样的路径/x,且小于最大长度
	ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);
	char *sub_path = (char*) pathname;
	struct directory *parent_dir = &root_dir;
	struct dir_entry dir_ent;
	// "/a/b/c" --> {"a", "b", "c"}
	char name[MAX_FILENAME_LEN] = {0};
	path_record->parent_dir = parent_dir;
	path_record->f_type = FT_UNKNOWN;
	uint32_t parent_inode_no = 0; // 父目录的inode号
	sub_path = parse_path(sub_path, name);
	while(name[0]) { // 若第一个字符就是结束符,结束循环
		ASSERT(strlen(path_record->searched_path) < 512);
		// 记录已存在的父目录
		strcat(path_record->searched_path, "/");
		strcat(path_record->searched_path, name);
		//  在所给的目录中查找文件
		if(search_dir_entry(cur_part, parent_dir, name, &dir_ent)) {
			memset(name, 0, MAX_FILENAME_LEN);
			// 若sub_path不等于NULL,也就是未结束时继续拆分路径
			if(sub_path) {
				sub_path = parse_path(sub_path, name);
			}
			if(FT_DIRECTORY == dir_ent.f_type) { // 目录
				parent_inode_no = parent_dir->inode->i_no;
				dir_close(parent_dir);
				 // 更新父目录
				parent_dir = dir_open(cur_part, dir_ent.i_no);
				path_record->parent_dir = parent_dir;
				continue;
			} else if(FT_FILE == dir_ent.f_type) { // 文件
				path_record->f_type = FT_FILE;
				return dir_ent.i_no;
			}
		} else { // 若找不到,则返回-1
			// 找不到目录项,parent_dir不要关闭,
			// 若是创建新文件,需要在parent_dir中创建
			return -1;
		}
	}
	// 执行到这里,肯定是遍历了完整的路径
	// 并且查找的文件或目录只有同名目录存在
	dir_close(path_record->parent_dir);
	// 保存被查找目录的直接父目录
	path_record->parent_dir = dir_open(cur_part, parent_inode_no);
	path_record->f_type = FT_DIRECTORY;
	return dir_ent.i_no;
}

// 打开或创建文件成功后,返回文件描述符,否则返回-1
int32_t sys_open(const char *pathname, enum file_option f_opt) {
	// 对目录要用dir_open
	if(pathname[strlen(pathname) - 1] == '/') {
		printk("can not open a directory %s!\n", pathname);
		return -1;
	}
	int32_t fd = -1;
	struct path_record path_record;
	memset(&path_record, 0, sizeof(struct path_record));
	// 记录目录深度,用于判断某个目录不存在的情况
	uint32_t dir_depth = get_path_depth((char*) pathname);
	// 先检查文件是否存在
	int32_t inode_no = search_file(pathname, &path_record);
	bool found = (inode_no != -1 ? true : false);
	if(path_record.f_type == FT_DIRECTORY) {
		printk("can not open a directory, use open dir to instead!\n");
		dir_close(path_record.parent_dir);
		return -1;
	}
	uint32_t searched_depth = get_path_depth(path_record.searched_path);
	// 若在访问某个中间目录过程中失败了
	if(dir_depth != searched_depth) {
		// 中间目录不存在
		printk("can not access %s : no such directory, subpath %s is not exist!\n", \
			pathname, path_record.searched_path);
		dir_close(path_record.parent_dir);
		return -1;
	}
	// 若是在最后一个路径上没找到，并且不是要创建文件,直接返回-1
	if(!found && !(f_opt & FO_CREATE)) {
		printk("in path %s, file %s is not exist!\n", path_record.searched_path, \
			strrchr(path_record.searched_path, '/') + 1);
		dir_close(path_record.parent_dir);
		return -1;
	} else if(found && (f_opt & FO_CREATE)) { // 若要创建的文件已存在
		printk("%s has already exist!\n", pathname);
		dir_close(path_record.parent_dir);
		return -1;
	}
	if(f_opt & FO_CREATE) {
		printk("creating file......\n");
		fd = file_create(path_record.parent_dir, strrchr(pathname, '/') + 1, f_opt);
		dir_close(path_record.parent_dir);
	} else { // 其余情况均为打开已存在文件
		fd = file_open(inode_no, f_opt);
	}
	// 此fd是task->fd_table数组中的元素下标
	return fd;
}

// 将文件描述符转化为文件表的下标
static uint32_t fd_local2global(uint32_t local_fd) {
	struct task_struct *cur_thread = current_thread();
	int32_t global_fd = cur_thread->fd_table[local_fd];
	ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);
	return (uint32_t) global_fd;
}

// 关闭文件描述符fd指向的文件,成功返回0,失败返回-1
int32_t sys_close(int32_t fd) {
	int32_t ret = -1;
	if(fd > 2) {
		uint32_t global_fd = fd_local2global(fd);
		ret = file_close(&file_table[global_fd]);
		current_thread()->fd_table[fd] = -1; // 使该文件描述符位可用
	}
	return ret;
}

// 将buf中连续count个字节写入文件描述符fd,
// 成功返回写入的字节数,失败返回-1
int32_t sys_write(int32_t fd, const void *buf, uint32_t count) {
	if(fd < 0) {
		printk("sys_write : fd error!\n");
		return -1;
	}
	if(fd == STDOUT_FD) {
		char tmp_buf[1024] = {0};
		memcpy(tmp_buf, buf, count);
		console_printk(tmp_buf);
		return count;
	}
	uint32_t global_fd = fd_local2global(fd);
	struct  file *file = &file_table[global_fd];
	if((file->fd_flag & FO_WRITEONLY) || (file->fd_flag & FO_READWRITE)) {
		uint32_t written_bytes = file_write(file, buf, count);
		return written_bytes;
	} else {
		console_printk("sys_write : not allowed to write file without flag FO_WRITEONLY or FO_READWRITE\n");
		return -1;
	}
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
		while(dev_no < 2) { // 目前只有一块硬盘100MB
			if(dev_no == 0) { // 跳过主盘
				++dev_no;
				continue;
			}
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
					ide_read(disk, part->lba_start + 1, sp_block, 1);
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
	char default_part[] = "sdb1";
	// 挂载分区
	list_traversal(&partition_list, partition_mount, (int) default_part);
	// 将当前分区的根目录打开
	open_root_dir(cur_part);
	// 初始化文件表
	for(uint32_t i = 0; i < MAX_FILE_OPEN; i++) {
		file_table[i].fd_inode = NULL;
	}
}

























































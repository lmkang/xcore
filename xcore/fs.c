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

// 从文件描述符fd指向的文件中读取count个字节到buf,
// 成功返回读出的字节数,若到文件尾返回-1
int32_t sys_read(int32_t fd, void *buf, uint32_t count) {
	if(fd < 0) {
		printk("sys_read : fd error!\n");
		return -1;
	}
	ASSERT(buf != NULL);
	uint32_t global_fd = fd_local2global(fd);
	return file_read(&file_table[global_fd], buf, count);
}

// 重置用于文件读写的偏移指针
// 成功返回新的偏移量,出错返回-1
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence) {
	if(fd < 0) {
		printk("sys_lseek : fd error!\n");
		return -1;
	}
	ASSERT(whence > 0 && whence < 4);
	uint32_t global_fd = fd_local2global(fd);
	struct file *file = &file_table[global_fd];
	int32_t new_pos = 0;
	int32_t file_size = (int32_t) file->fd_inode->i_size;
	switch(whence) {
		// SEKK_SET : 新的读写位置是相对于文件开头再增加offset个位移量
		case SEEK_SET:
			new_pos = offset;
			break;
		// SEKK_CUR : 新的读写位置是相对于当前的位置增加offset个位移量
		case SEEK_CUR:
			new_pos = (int32_t) file->fd_pos + offset;
			break;
		// SEEK_END : 新的读写位置是相对于文件尺寸再增加offset个位移量
		case SEEK_END:
			new_pos = file_size + offset;
			break;
	}
	if(new_pos < 0 || new_pos > (file_size - 1)) {
		return -1;
	}
	file->fd_pos = new_pos;
	return file->fd_pos;
}

// 删除文件(非目录),成功返回0,失败返回-1
int32_t sys_unlink(const char *pathname) {
	ASSERT(strlen(pathname) < MAX_PATH_LEN);
	// 先检查待删除的文件是否存在
	struct path_record path_record;
	memset(&path_record, 0, sizeof(struct path_record));
	int inode_no = search_file(pathname, &path_record);
	ASSERT(inode_no != 0);
	if(inode_no == -1) {
		printk("file %s not found!\n", pathname);
		dir_close(path_record.parent_dir);
		return -1;
	}
	if(path_record.f_type == FT_DIRECTORY) {
		printk("can not delete a directory by unlink(), using rmdir() instead\n");
		dir_close(path_record.parent_dir);
		return -1;
	}
	// 检查是否在已打开文件列表(文件表)中
	uint32_t file_index = 0;
	while(file_index < MAX_FILE_OPEN) {
		if(file_table[file_index].fd_inode != NULL
			&& (uint32_t) inode_no == file_table[file_index].fd_inode->i_no) {
			break;
		}
		++file_index;
	}
	if(file_index < MAX_FILE_OPEN) {
		dir_close(path_record.parent_dir);
		printk("file %s is in use, not allow to delete!\n", pathname);
		return -1;
	}
	ASSERT(file_index == MAX_FILE_OPEN);
	// 为delete_dir_entry申请缓冲区
	void *io_buf = sys_malloc(SECTOR_SIZE * 2);
	if(io_buf == NULL) {
		dir_close(path_record.parent_dir);
		printk("sys_unlink : alloc memory failed!\n");
		return -1;
	}
	delete_dir_entry(cur_part, path_record.parent_dir, inode_no, io_buf);
	inode_release(cur_part, inode_no);
	sys_free(io_buf);
	dir_close(path_record.parent_dir);
	return 0;
}

// 创建目录pathname,成功返回0,失败返回-1
int32_t sys_mkdir(const char *pathname) {
	uint8_t rollback_flag = 0; // 操作失败回滚标志
	void *io_buf = sys_malloc(SECTOR_SIZE * 2);
	if(io_buf == NULL) {
		printk("sys_mkdir : alloc memory failed!\n");
		return -1;
	}
	struct path_record path_record;
	memset(&path_record, 0, sizeof(struct path_record));
	int inode_no = -1;
	inode_no = search_file(pathname, &path_record);
	if(inode_no == -1) { // 如果找到了同名目录或文件
		printk("sys_mkdir : file or directory %s exist!\n", pathname);
		rollback_flag = 1;
		goto rollback;
	} else {
		// 未找到,判断是否是某个中间目录不存在
		uint32_t pathname_depth = get_path_depth((char*) pathname);
		uint32_t searched_depth = get_path_depth(path_record.searched_path);
		// 先判断是否在某个中间目录就失败了
		if(pathname_depth != searched_depth) {
			printk("sys_mkdir : can not access %s : not a directory, \
				subpath %s is not exist!\n", pathname, path_record.searched_path);
			rollback_flag = 1;
			goto rollback;
		}
	}
	struct directory *parent_dir = path_record.parent_dir;
	// 目录名称后可能有"/"
	// 最好用path_record.searched_record,无'/'
	char *dir_name = strrchr(path_record.searched_path, '/') + 1;
	inode_no = alloc_inode_bitmap(cur_part);
	if(inode_no == -1) {
		printk("sys_mkdir : alloc inode failed!\n");
		rollback_flag = 1;
		goto rollback;
	}
	struct inode new_dir_inode;
	inode_init(inode_no, &new_dir_inode);
	uint32_t block_btmp_index = 0;
	int32_t block_lba = -1;
	// 为目录分配一个块,用来写入目录.和..
	block_lba = alloc_block_bitmap(cur_part);
	if(block_lba == -1) {
		printk("sys_mkdir : alloc_block_bitmap failed!\n");
		rollback_flag = 2;
		goto rollback;
	}
	new_dir_inode.sectors[0] = block_lba;
	// 每分配一个块就将位图同步到硬盘
	block_btmp_index = block_lba - cur_part->sp_block->data_lba_start;
	ASSERT(block_btmp_index != 0);
	bitmap_sync(cur_part, block_btmp_index, BLOCK_BITMAP);
	// 将当前目录的目录项.和..写入目录
	memset(io_buf, 0, SECTOR_SIZE * 2);
	struct dir_entry *p_dir_ent = (struct dir_entry*) io_buf;
	// 初始化当前目录.
	memcpy(p_dir_ent->filename, ".", 1);
	p_dir_ent->i_no = inode_no;
	p_dir_ent->f_type = FT_DIRECTORY;
	++p_dir_ent;
	// 初始化当前目录..
	memcpy(p_dir_ent->filename, "..", 2);
	p_dir_ent->i_no = parent_dir->inode->i_no;
	p_dir_ent->f_type = FT_DIRECTORY;
	ide_write(cur_part->disk, new_dir_inode.sectors[0], io_buf, 1);
	new_dir_inode.i_size = 2 * cur_part->sp_block->dir_entry_size;
	// 在父目录添加自己的目录项
	struct dir_entry new_dir_entry;
	memset(&new_dir_entry, 0, sizeof(struct dir_entry));
	create_dir_entry(dir_name, inode_no, FT_DIRECTORY, &new_dir_entry);
	memset(io_buf, 0, SECTOR_SIZE * 2);
	if(!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
		printk("sys_mkdir : sync_dir_entry failed!\n");
		rollback_flag = 2;
		goto rollback;
	}
	// 父目录的inode同步到硬盘
	memset(io_buf, 0, SECTOR_SIZE * 2);
	inode_sync(cur_part, parent_dir->inode, io_buf);
	// 将新创建目录的inode同步到硬盘
	memset(io_buf, 0, SECTOR_SIZE * 2);
	inode_sync(cur_part, &new_dir_inode, io_buf);
	// 将inode位图同步到硬盘
	bitmap_sync(cur_part, inode_no, INODE_BITMAP);
	sys_free(io_buf);
	// 关闭所创建目录的父目录
	dir_close(path_record.parent_dir);
	return 0;
rollback:
	switch(rollback_flag) {
		case 2:
			// 如果新文件的inode创建失败,之前位图中分配的inode_no也要恢复
			set_bitmap(&cur_part->inode_btmp, inode_no, 0);
		case 1:
			// 关闭所创建目录的父目录
			dir_close(path_record.parent_dir);
			break;
	}
	sys_free(io_buf);
	return -1;
}

// 目录打开成功后返回目录指针,失败返回NULL
struct directory *sys_opendir(const char *name) {
	ASSERT(strlen(name) < MAX_PATH_LEN);
	// 如果是根目录'/',直接返回&root_dir
	if(name[0] == '/' && (name[1] == 0 || name[0] == '.')) {
		return &root_dir;
	}
	// 先检查待打开的目录是否存在
	struct path_record path_record;
	memset(&path_record, 0, sizeof(struct path_record));
	int inode_no = search_file(name, &path_record);
	struct directory *ret_dir = NULL;
	if(inode_no == -1) { // 找不到目录,提示不存在的路径
		printk("in %s, subpath %s is not exist!\n", name, path_record.searched_path);
	} else {
		if(path_record.f_type == FT_FILE) {
			printk("%s is file, not directory!\n", name);
		} else if(path_record.f_type == FT_DIRECTORY) {
			ret_dir = dir_open(cur_part, inode_no);
		}
	}
	dir_close(path_record.parent_dir);
	return ret_dir;
}

// 关闭目录,成功返回0,失败返回-1
int32_t sys_closedir(struct directory *dir) {
	int32_t ret = -1;
	if(dir != NULL) {
		dir_close(dir);
		ret = 0;
	}
	return ret;
}

// 读取目录dir的一个目录项
// 成功返回目录项地址,失败返回NULL
struct dir_entry *sys_readdir(struct directory *dir) {
	ASSERT(dir != NULL);
	return dir_read(dir);
}

// 把目录dir的指针dir_pos置0
void sys_rewinddir(struct directory *dir) {
	dir->dir_pos = 0;
}

// 删除空目录
// 成功返回0,失败返回-1
int32_t sys_rmdir(const char *pathname) {
	// 先检查待删除的文件是否存在
	struct path_record path_record;
	memset(&path_record, 0, sizeof(struct path_record));
	int inode_no = search_file(pathname, &path_record);
	ASSERT(inode_no != 0);
	int ret_val = -1; // 默认返回值
	if(inode_no == -1) {
		printk("in %s, subpath %s is not exist!\n", pathname, path_record.searched_path);
	} else {
		if(path_record.f_type == FT_FILE) {
			printk("%s is file, not a directory!\n", pathname);
		} else {
			struct directory *dir = dir_open(cur_part, inode_no);
			if(!dir_empty(dir)) { // 非空目录不可删除
				printk("directory %s si not empty!", pathname);
			} else {
				if(!dir_remove(path_record.parent_dir, dir)) {
					ret_val = 0;
				}
			}
			dir_close(dir);
		}
	}
	dir_close(path_record.parent_dir);
	return ret_val;
}

// 获得父目录的inode编号
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr, void *io_buf) {
	struct inode *child_dir_inode = inode_open(cur_part, child_inode_nr);
	// 目录中的目录项".."中包括父目录inode编号,".."位于目录的第0块
	uint32_t block_lba = child_dir_inode->sectors[0];
	ASSERT(block_lba >= cur_part->sp_block->data_lba_start);
	inode_close(child_dir_inode);
	ide_read(cur_part->disk, block_lba, io_buf, 1);
	struct dir_entry *dir_ent = (struct dir_entry*) io_buf;
	// 第0个目录项是".",第1个目录项是".."
	ASSERT(dir_ent[1].i_no < 4096 && dir_ent[1].f_type == FT_DIRECTORY);
	return dir_ent[1].i_no; // 返回..即父目录的inode编号
}

// 在inode编号为p_inode_nr的目录中查找
// inode编号为c_inode_nr的子目录的名字
// 将名字存入缓冲区path,成功返回0,失败返回-1
static int get_child_dir_name(uint32_t p_inode_nr, uint32_t c_inode_nr, \
	char *path, void *io_buf) {
	struct inode *parent_dir_inode = inode_open(cur_part, p_inode_nr);
	// 填充all_blocks,将该目录的所占扇区地址全部写入all_blocks
	uint8_t block_index = 0;
	uint32_t all_blocks[140] = {0};
	uint32_t block_cnt = 12;
	while(block_index < 12) {
		all_blocks[block_index] = parent_dir_inode->sectors[block_index];
		++block_index;
	}
	// 若包含一级间接块表,将其读入all_blocks
	if(parent_dir_inode->sectors[12] != 0) {
		ide_read(cur_part->disk, parent_dir_inode->sectors[12], all_blocks + 12, 1);
		block_cnt = 140;
	}
	inode_close(parent_dir_inode);
	struct dir_entry *dir_ent = (struct dir_entry*) io_buf;
	uint32_t dir_entry_size = cur_part->sp_block->dir_entry_size;
	uint32_t dir_entry_count = SECTOR_SIZE / dir_entry_size;
	block_index = 0;
	// 遍历所有块
	while(block_index < block_cnt) {
		if(all_blocks[block_index] != 0) { // 若相应块不为空,则读入相应块
			ide_read(cur_part->disk, all_blocks[block_index], io_buf, 1);
			uint8_t dir_entry_index = 0;
			// 遍历每个目录项
			while(dir_entry_index < dir_entry_count) {
				if((dir_ent + dir_entry_index)->i_no == c_inode_nr) {
					strcat(path, "/");
					strcat(path, (dir_ent + dir_entry_index)->filename);
					return 0;
				}
				++dir_entry_index;
			}
		}
		++block_index;
	}
	return -1;
}

// 把当前工作目录绝对路径写入buf,size是buf的大小
// 当buf为NULL时,由操作系统分配存储工作路径的空间并返回地址
// 失败则返回NULL
char *sys_getcwd(char *buf, uint32_t size) {
	// 确保buf不为空,若用户进程提供的buf为NULL,
	// 系统调用getcwd中要为用户进程通过malloc分配内存
	ASSERT(buf != NULL);
	void *io_buf = sys_malloc(SECTOR_SIZE);
	if(io_buf == NULL) {
		return NULL;
	}
	struct task_struct *cur_thread = current_thread();
	int32_t parent_inode_nr = 0;
	int32_t child_inode_nr = cur_thread->cwd_inode_nr;
	// 最大支持4096个inode
	ASSERT(child_inode_nr >= 0 && child_inode_nr < 4096);
	// 若当前目录是根目录,直接返回
	if(child_inode_nr == 0) {
		buf[0] = '/';
		buf[1] = 0;
		return buf;
	}
	memset(buf, 0, size);
	// 用来做全路径缓冲区
	char full_path_reverse[MAX_PATH_LEN] = {0};
	// 从下往上逐层找父目录,直到找到根目录为止
	// 当child_inode_nr为根目录的inode编号(0)时停止
	// 即已经查看完根目录中的目录项
	while(child_inode_nr) {
		parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr, io_buf);
		if(get_child_dir_name(parent_inode_nr, child_inode_nr, \
			full_path_reverse, io_buf) == -1) {
			sys_free(io_buf);
			return NULL;
		}
		child_inode_nr = parent_inode_nr;
	}
	ASSERT(strlen(full_path_reverse) <= size);
	// 至此full_path_reverse中的路径是反着的,
	// 即子目录在前(左),父目录在后(右)
	// 现将full_path_reverse的路径反置
	char *last_slash; // 记录字符串中的最后一个斜杠地址
	while((last_slash = strrchr(full_path_reverse, '/'))) {
		uint16_t len = strlen(buf);
		strcpy(buf + len, last_slash);
		// 在full_path_reverse中添加结束字符,
		// 作为下一次执行strcpy中last_slash的边界
		*last_slash = 0;
	}
	sys_free(io_buf);
	return buf;
}

// 更改当前工作目录为绝对路径path
// 成功返回0,失败返回-1
int32_t sys_chdir(const char *path) {
	int32_t ret_val = -1;
	struct path_record path_record;
	memset(&path_record, 0, sizeof(struct path_record));
	int inode_no = search_file(path, &path_record);
	if(inode_no != -1) {
		if(path_record.f_type == FT_DIRECTORY) {
			current_thread()->cwd_inode_nr = inode_no;
			ret_val = 0;
		} else {
			printk("sys_chdir : %s is a file, not a directory!\n", path);
		}
	}
	dir_close(path_record.parent_dir);
	return ret_val;
}

// 在buf中填充文件结构相关信息
// 成功返回0,失败返回-1
int32_t sys_stat(const char *path, struct file_stat *buf) {
	// 若直接查看根目录'/'
	if(!strcmp(path, "/") || !strcmp(path, "/.") || !strcmp(path, "/..")) {
		buf->f_type = FT_DIRECTORY;
		buf->i_no = 0;
		buf->size = root_dir.inode->i_size;
		return 0;
	}
	int32_t ret_val = -1; // 默认返回值
	struct path_record path_record;
	// 记得初始化或清0,否则栈中信息不知道是什么
	memset(&path_record, 0, sizeof(struct path_record));
	int inode_no = search_file(path, &path_record);
	if(inode_no != -1) {
		// 只为获得文件大小
		struct inode *obj_inode = inode_open(cur_part, inode_no);
		buf->size = obj_inode->i_size;
		inode_close(obj_inode);
		buf->f_type = path_record.f_type;
		buf->i_no = inode_no;
		ret_val = 0;
	} else {
		printk("sys_stat : %s not found!\n", path);
	}
	dir_close(path_record.parent_dir);
	return ret_val;
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

























































#include "types.h"
#include "thread.h"
#include "print.h"
#include "syscall.h"
#include "string.h"
#include "memory.h"
#include "fs.h"
#include "fork.h"

#define SYSCALL_COUNT 32

void *syscall_table[SYSCALL_COUNT];

// 清屏
extern void sys_clear(void);

// 无参数的系统调用
#define _syscall0(NUMBER) ({ \
	int value; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(value) \
		: "a"(NUMBER) \
		: "memory" \
	); \
	value; \
})

// 一个参数的系统调用
#define _syscall1(NUMBER, ARG1) ({ \
	int value; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(value) \
		: "a"(NUMBER), "b"(ARG1) \
		: "memory" \
	); \
	value; \
})

// 两个参数的系统调用
#define _syscall2(NUMBER, ARG1, ARG2) ({ \
	int value; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(value) \
		: "a"(NUMBER), "b"(ARG1), "c"(ARG2) \
		: "memory" \
	); \
	value; \
})

// 三个参数的系统调用
#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({ \
	int value; \
	__asm__ __volatile__( \
		"int $0x80" \
		: "=a"(value) \
		: "a"(NUMBER), "b"(ARG1), "c"(ARG2), "d"(ARG3) \
		: "memory" \
	); \
	value; \
})

// 返回当前任务的pid
pid_t sys_getpid(void) {
	return current_thread()->pid;
}

// 初始化系统调用
void syscall_init(void) {
	syscall_table[SYS_GETPID] = sys_getpid;
	syscall_table[SYS_WRITE] = sys_write;
	syscall_table[SYS_MALLOC] = sys_malloc;
	syscall_table[SYS_FREE] = sys_free;
	syscall_table[SYS_FORK] = sys_fork;
	syscall_table[SYS_READ] = sys_read;
	syscall_table[SYS_PUTCHAR] = console_put_char;
	syscall_table[SYS_CLEAR] = sys_clear;
	syscall_table[SYS_GETCWD] = sys_getcwd;
	syscall_table[SYS_OPEN] = sys_open;
	syscall_table[SYS_CLOSE] = sys_close;
	syscall_table[SYS_LSEEK] = sys_lseek;
	syscall_table[SYS_UNLINK] = sys_unlink;
	syscall_table[SYS_MKDIR] = sys_mkdir;
	syscall_table[SYS_OPENDIR] = sys_opendir;
	syscall_table[SYS_CLOSEDIR] = sys_closedir;
	syscall_table[SYS_CHDIR] = sys_chdir;
	syscall_table[SYS_RMDIR] = sys_rmdir;
	syscall_table[SYS_READDIR] = sys_readdir;
	syscall_table[SYS_REWINDDIR] = sys_rewinddir;
	syscall_table[SYS_STAT] = sys_stat;
	syscall_table[SYS_PS] = sys_ps;
	
	printk("syscall_init done\n");
}

// -------- user program call ------------

// 返回当前任务pid
pid_t getpid(void) {
	return _syscall0(SYS_GETPID);
}

// 打印字符串
uint32_t write(int32_t fd, const void *buf, uint32_t count) {
	return _syscall3(SYS_WRITE, fd, buf, count);
}

// 申请size字节大小的内存
void *malloc(uint32_t size) {
	return (void*) _syscall1(SYS_MALLOC, size);
}

// 释放ptr指向的内存
void free(void *ptr) {
	_syscall1(SYS_FREE, ptr);
}

// fork
pid_t fork(void) {
	return _syscall0(SYS_FORK);
}

// 从文件描述符fd中读取count个字节到buf
int32_t read(int32_t fd, void *buf, uint32_t count) {
	return _syscall3(SYS_READ, fd, buf, count);
}

// 终端输出一个字符
void putchar(char ch) {
	_syscall1(SYS_PUTCHAR, ch);
}

// 清屏
void clear(void) {
	_syscall0(SYS_CLEAR);
}

// 获取当前工作目录
char *getcwd(char *buf, uint32_t size) {
	return (char*) _syscall2(SYS_GETCWD, buf, size);
}

// 以flag方式打开文件pathname
int32_t open(char *pathname, uint8_t flag) {
	return _syscall2(SYS_OPEN, pathname, flag);
}

// 关闭文件fd
int32_t close(int32_t fd) {
	return _syscall1(SYS_CLOSE, fd);
}

// 设置文件偏移量
int32_t lseek(int32_t fd, int32_t offset, uint8_t whence) {
	return _syscall3(SYS_LSEEK, fd, offset, whence);
}

// 删除文件pathname
int32_t unlink(const char *pathname) {
	return _syscall1(SYS_UNLINK, pathname);
}

// 创建目录pathname
int32_t mkdir(const char *pathname) {
	return _syscall1(SYS_MKDIR, pathname);
}

// 打开目录name
struct directory *opendir(const char *name) {
	return (struct directory*) _syscall1(SYS_OPENDIR, name);
}

// 关闭目录dir
int32_t closedir(struct directory *dir) {
	return _syscall1(SYS_CLOSEDIR, dir);
}

// 删除目录pathname
int32_t rmdir(const char *pathname) {
	return _syscall1(SYS_RMDIR, pathname);
}

// 读取目录dir
struct dir_entry *readdir(struct directory *dir) {
	return (struct dir_entry*) _syscall1(SYS_READDIR, dir);
}

// 回归目录指针
void rewinddir(struct directory *dir) {
	_syscall1(SYS_REWINDDIR, dir);
}

// 获取path属性到buf中
int32_t stat(const char *path, struct file_stat *buf) {
	return _syscall2(SYS_STAT, path, buf);
}

// 改变工作目录为path
int32_t chdir(const char *path) {
	return _syscall1(SYS_CHDIR, path);
}

// 显示任务列表
void ps(void) {
	_syscall0(SYS_PS);
}






















































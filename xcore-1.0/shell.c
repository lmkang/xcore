#include "syscall.h"
#include "stdio.h"
#include "string.h"
#include "file.h"
#include "command.h"

// 加上命令名外,最多支持15个参数
#define MAX_ARG_COUNT 16

// 存储输入的命令
static char cmd_line[MAX_PATH_LEN] = {0};

// 用来记录当前目录,是当前目录的缓存,
// 每次执行cd命令时会更新此内容
char cwd_cache[MAX_PATH_LEN] = {0};

// 为了给exec的程序访问参数
char *argv[MAX_ARG_COUNT];

int32_t argc = -1;

// 最终路径
char final_path[MAX_PATH_LEN] = {0};

// 输入提示符
void print_prompt(void) {
	printf("[lmkang@localhost %s]$ ", cwd_cache);
}

// 从键盘缓冲区中最多读入count个字节到buf
static void readline(char *buf, int32_t count) {
	char *pos = buf;
	while(read(STDIN_FD, pos, 1) != -1 && (pos - buf) < count) {
		// 在不出错的情况下,直到找到回车符才返回
		switch(*pos) {
			case '\n':
			case '\r':
				*pos = 0; // 添加cmd_line的终止字符0
				putchar('\n');
				return;
			case '\b':
				if(buf[0] != '\b') { // 阻止删除非本次输入的信息
					--pos; // 退回到缓冲区cmd_line中的上一个字符
					putchar('\b');
				}
				break;
			// ctrl + l清屏
			case 'l' - 'a':
				// 1 先将当前的字符'l' - 'a'置为0
				*pos = 0;
				// 2 再将屏幕清空
				clear();
				// 3 打印提示符
				print_prompt();
				// 4 将之前键入的内容再次打印
				printf("%s", buf);
				break;
			// ctrl + u清掉键入
			case 'u' - 'a':
				while(buf != pos) {
					putchar('\b');
					*(pos--) = 0;
				}
				break;
			// 非控制键则输入字符
			default:
				putchar(*pos);
				++pos;
		}
	}
	printf("readline: can not find enter key in the cmd line, max number of char is 128!\n");
}

// 解析字符串cmd_str中以token为分隔符的单词,
// 并将各单词的指针存入argv数组
static int32_t parse_cmd(char *cmd_str, char **argv, char token) {
	int32_t arg_index = 0;
	while(arg_index < MAX_ARG_COUNT) {
		argv[arg_index] = NULL;
		++arg_index;
	}
	char *next = cmd_str;
	int32_t argc = 0;
	// 外层循环处理整个命令行
	while(*next) {
		// 去除命令字或参数之间的空格
		while(*next == token) {
			++next;
		}
		// 处理最后一个参数后接空格的情况
		if(*next == 0) {
			break;
		}
		argv[argc] = next;
		// 内层循环处理命令行中的每个命令字和参数
		while(*next && *next != token) { // 在字符串结束前找单词分隔符
			++next;
		}
		// 如果未结束(token字符),使token变成0
		if(*next) {
			// 将token字符替换为字符串结束符0,
			// 作为一个单词的结束,并将字符指针next指向下一个字符
			*next++ = 0;
		}
		// 避免argv数组访问越界,参数过多则返回0
		if(argc > MAX_ARG_COUNT) {
			return -1;
		}
		++argc;
	}
	return argc;
}

// 简单的shell
void simple_shell(void) {
	cwd_cache[0] = '/';
	while(1) {
		print_prompt();
		memset(final_path, 0, MAX_PATH_LEN);
		memset(cmd_line, 0, MAX_PATH_LEN);
		readline(cmd_line, MAX_PATH_LEN);
		if(cmd_line[0] == 0) { // 若只键入一个回车
			continue;
		}
		argc = -1;
		argc = parse_cmd(cmd_line, argv, ' ');
		if(argc == -1) {
			printf("number of arg exceed %d\n", MAX_ARG_COUNT);
			continue;
		}
		if(!strcmp("ls", argv[0])) {
			cmd_ls(argc, argv);
		} else if(!strcmp("cd", argv[0])) {
			if(cmd_cd(argc, argv) != NULL) {
				memset(cwd_cache, 0, MAX_PATH_LEN);
				strcpy(cwd_cache, final_path);
			}
		} else if(!strcmp("pwd", argv[0])) {
			cmd_pwd(argc, argv);
		} else if(!strcmp("ps", argv[0])) {
			cmd_ps(argc, argv);
		} else if(!strcmp("clear", argv[0])) {
			cmd_clear(argc, argv);
		} else if(!strcmp("mkdir", argv[0])) {
			cmd_mkdir(argc, argv);
		} else if(!strcmp("rmdir", argv[0])) {
			cmd_rmdir(argc, argv);
		} else if(!strcmp("rm", argv[0])) {
			cmd_rm(argc, argv);
		} else {
			printf("unknown command\n");
		}
	}
	printf("simple_shell error!\n");
}

























































































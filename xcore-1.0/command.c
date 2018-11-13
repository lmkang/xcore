#include "types.h"
#include "fs.h"
#include "syscall.h"
#include "string.h"
#include "stdio.h"
#include "file.h"

// 最终路径
extern char final_path[MAX_PATH_LEN];

// 将路径old_abs_path中的..和.转换为实际路径后存入new_abs_path
static void wash_path(char *old_abs_path, char *new_abs_path) {
	char name[MAX_FILENAME_LEN] = {0};
	char *sub_path = old_abs_path;
	sub_path = parse_path(sub_path, name);
	// 若只键入了"/",直接将"/"存入new_abs_path后返回
	if(name[0] == 0) {
		new_abs_path[0] = '/';
		new_abs_path[1] = 0;
		return;
	}
	new_abs_path[0] = 0; // 避免传给new_abs_path的缓冲区不干净
	strcat(new_abs_path, "/");
	while(name[0]) {
		if(!strcmp("..", name)) { // 如果是上一级目录".."
			char *slash_ptr = strrchr(new_abs_path, '/');
			// 如果未到new_abs_path中的顶层目录,就将最右边的'/'替换为0,
			// 即去除了new_abs_path的最后一层路径,相当于到了上一级目录
			if(slash_ptr != new_abs_path) { // 如果: "/a/b" ---> "/a"
				*slash_ptr = 0;
			} else { // 如果: "/a" ---> "/"
				// 若new_abs_path中只有1个'/',表示已经到了顶层目录
				// 那么就将下一个字符置为结束符0
				*(slash_ptr + 1) = 0;
			}
		} else if(strcmp(".", name)) { // 路径不是'.'
			// 拼接name到new_abs_path
			if(strcmp(new_abs_path, "/")) {
				// new_abs_path不是"/"
				// 就拼接一个"/",避免路径开头变成"//"
				strcat(new_abs_path, "/");
			}
			strcat(new_abs_path, name);
		}
		// 若name是当前目录".",无需处理new_abs_path
		// 继续遍历下一层路径
		memset(name, 0, MAX_FILENAME_LEN);
		if(sub_path) {
			sub_path = parse_path(sub_path, name);
		}
	}
}

// 将path处理成不含".."和"."的绝对路径,存储在final_path
void process_abs_path(char *path, char *final_path) {
	char abs_path[MAX_PATH_LEN] = {0};
	// 先判断是不是绝对路径
	if(path[0] != '/') {
		memset(abs_path, 0, MAX_PATH_LEN);
		if(getcwd(abs_path, MAX_PATH_LEN) != NULL) {
			// abs_path表示的当前目录不是根目录
			if(!(abs_path[0] == '/' && abs_path[1] == 0)) {
				strcat(abs_path, "/");
			}
		}
	}
	strcat(abs_path, path);
	wash_path(abs_path, final_path);
}

// pwd命令
void cmd_pwd(uint32_t argc, __attribute__((unused))char **argv) {
	if(argc != 1) {
		printf("pwd: no arg support!\n");
		return;
	} else {
		if(getcwd(final_path, MAX_PATH_LEN) != NULL) {
			printf("%s\n", final_path);
		} else {
			printf("pwd: get current work directory failed!\n");
		}
	}
}

// cd命令
char *cmd_cd(uint32_t argc, char **argv) {
	if(argc > 2) {
		printf("cd: only support 1 arg!\n");
		return NULL;
	} else {
		if(argc == 1) { // 如果只键入cd,直接返回到根目录
			final_path[0] = '/';
			final_path[1] = 0;
		} else {
			process_abs_path(argv[1], final_path);
		}
		if(chdir(final_path) == -1) {
			printf("cd: no such directory %s\n", final_path);
			return NULL;
		}
		return final_path;
	}
}

// ls命令
void cmd_ls(uint32_t argc, char **argv) {
	char *pathname = NULL;
	struct file_stat f_stat;
	memset(&f_stat, 0, sizeof(struct file_stat));
	bool long_info = false;
	uint32_t arg_path_count = 0;
	uint32_t arg_index = 1; // 跨过argv[0], argv[0]是ls
	while(arg_index < argc) {
		if(argv[arg_index][0] == '-') { // 若是选项,首字符应该是'-'
			if(!strcmp("-l", argv[arg_index])) { // "-l"
				long_info = true;
			} else if(!strcmp("-h", argv[arg_index])) { // "-h"
				printf("usage: -l list all infomation\n-h for the command help\n");
				return;
			} else { // not supported option
				printf("ls: invalid option %s, try ls -h for more infomation\n", argv[arg_index]);
				return;
			}
		} else { // ls的路径参数
			if(arg_path_count == 0) {
				pathname = argv[arg_index];
				arg_path_count = 1;
			} else {
				printf("ls: only support one path\n");
				return;
			}
		}
		++arg_index;
	}
	if(pathname == NULL) { // 只输入ls或ls -l
		// 没有输入路径,默认以当前路径的绝对路径作为参数
		if(getcwd(final_path, MAX_PATH_LEN) != NULL) {
			pathname = final_path;
		} else {
			printf("ls: getcwd() failed!\n");
			return;
		}
	} else {
		process_abs_path(pathname, final_path);
		pathname = final_path;
	}
	if(stat(pathname, &f_stat) == -1) {
		printf("ls: can not access %s: No such file or directory\n", pathname);
		return;
	}
	if(f_stat.f_type == FT_DIRECTORY) {
		struct directory *dir = opendir(pathname);
		struct dir_entry *dir_ent = NULL;
		char sub_pathname[MAX_PATH_LEN] = {0};
		uint32_t pathname_len = strlen(pathname);
		uint32_t last_char_index = pathname_len - 1;
		memcpy(sub_pathname, pathname, pathname_len);
		if(sub_pathname[last_char_index] != '/') {
			sub_pathname[pathname_len] = '/';
			++pathname_len;
		}
		rewinddir(dir);
		if(long_info) {
			char ftype;
			printf("total: %d\n", f_stat.size);
			while((dir_ent = readdir(dir))) {
				ftype = 'd';
				if(dir_ent->f_type == FT_FILE) {
					ftype = '-';
				}
				sub_pathname[pathname_len] = 0;
				strcat(sub_pathname, dir_ent->filename);
				memset(&f_stat, 0, sizeof(struct file_stat));
				if(stat(sub_pathname, &f_stat) == -1) {
					printf("ls: can not access %s: No such file or directory\n", dir_ent->filename);
					return;
				}
				printf("%c    %d    %d    %s\n", ftype, dir_ent->i_no, f_stat.size, dir_ent->filename);
			}
		} else {
			while((dir_ent = readdir(dir))) {
				printf("%s ", dir_ent->filename);
			}
			printf("\n");
		}
		closedir(dir);
	} else {
		if(long_info) {
			printf("-    %d    %d    %s\n", f_stat.i_no, f_stat.size, pathname);
		} else {
			printf("%s\n", pathname);
		}
	}
}

// ps命令
void cmd_ps(uint32_t argc, __attribute__((unused))char **argv) {
	if(argc != 1) {
		printf("ps: no arg support!\n");
		return;
	}
	ps();
}

// clear命令
void cmd_clear(uint32_t argc, __attribute__((unused))char **argv) {
	if(argc != 1) {
		printf("clear: no arg support!\n");
		return;
	}
	clear();
}

// mkdir命令
int32_t cmd_mkdir(uint32_t argc, char **argv) {
	int32_t ret_val = -1;
	if(argc != 2) {
		printf("mkdir: only support 1 arg!\n");
	} else {
		process_abs_path(argv[1], final_path);
		// 若创建的不是根目录
		if(strcmp("/", final_path)) {
			if(mkdir(final_path) == 0) {
				ret_val = 0;
			} else {
				printf("mkdir: create directory %s failed\n", argv[1]);
			}
		}
	}
	return ret_val;
}

// rmdir命令
int32_t cmd_rmdir(uint32_t argc, char **argv) {
	int32_t ret_val = -1;
	if(argc != 2) {
		printf("rmdir: only support 1 arg!\n");
	} else {
		process_abs_path(argv[1], final_path);
		// 若删除的不是根目录
		if(strcmp("/", final_path)) {
			if(rmdir(final_path) == 0) {
				ret_val = 0;
			} else {
				printf("rmdir: remove %s failed\n", argv[1]);
			}
		}
	}
	return ret_val;
}

// rm命令
int32_t cmd_rm(uint32_t argc, char **argv) {
	int32_t ret_val = -1;
	if(argc != 2) {
		printf("rm: only support 1 arg!\n");
	} else {
		process_abs_path(argv[1], final_path);
		// 若删除的不是根目录
		if(strcmp("/", final_path)) {
			if(unlink(final_path) == 0) {
				ret_val = 0;
			} else {
				printf("rm: remove %s failed\n", argv[1]);
			}
		}
	}
	return ret_val;
}








































































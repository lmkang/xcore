#ifndef __COMMAND_H
#define __COMMAND_H

void process_abs_path(char *path, char *final_path);

void cmd_pwd(uint32_t argc, __attribute__((unused))char **argv);

char *cmd_cd(uint32_t argc, char **argv);

void cmd_ls(uint32_t argc, char **argv);

void cmd_ps(uint32_t argc, __attribute__((unused))char **argv);

void cmd_clear(uint32_t argc, __attribute__((unused))char **argv);

int32_t cmd_mkdir(uint32_t argc, char **argv);

int32_t cmd_rmdir(uint32_t argc, char **argv);

int32_t cmd_rm(uint32_t argc, char **argv);

#endif
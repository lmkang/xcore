#ifndef __USER_PROCESS_H 
#define __USE_PROCESS_H 
#include "thread.h"
#include "stdint.h"

#define DEFAULT_PRIORITY 31
#define USER_STACK3_VADDR  (0xc0000000 - 0x1000)
#define USER_VADDR_START 0x8048000

void process_start(void *filename);

void page_dir_activate(struct task_struct *pthread);

void process_activate(struct task_struct *pthread);

uint32_t * create_page_dir(void);

void create_user_vaddr_bitmap(struct task_struct *user_task);

void process_execute(void *filename, char *name);

#endif
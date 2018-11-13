#ifndef __PROCESS_H
#define __PROCESS_H

void start_process(void *filename);

void pgdir_activate(struct task_struct *pthread);

void process_activate(struct task_struct *pthread);

uint32_t *create_pgdir(void);

void create_user_vaddr_bitmap(struct task_struct *uprog);

void process_execute(void *filename, char *name);

#endif
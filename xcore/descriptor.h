#ifndef __GDT_H
#define __GDT_H

void update_tss_esp(struct task_struct *pthread);

void init_gdt();

#endif
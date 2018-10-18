#ifndef __MEMORY_H
#define __MEMORY_H

void init_kernel_vmm();

void init_mem_pool(uint32_t mem_size);

void *kmalloc(uint32_t size);

#endif
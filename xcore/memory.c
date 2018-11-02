#include "types.h"
#include "global.h"
#include "print.h"
#include "bitmap.h"
#include "debug.h"
#include "memory.h"
#include "sync.h"
#include "thread.h"
#include "string.h"
#include "interrupt.h"

// 内存仓库
struct arena {
	struct mem_block_desc *desc;
	// large为true时,count表示页框数
	// 否则表示空闲mem_block数量
	uint32_t count;
	bool large;
};

// 物理内存池
struct memory_pool {
	struct bitmap pool_btmp;
	uint32_t paddr_start;
	uint32_t pool_size; // 字节大小
	struct lock lock;
};

// 内核页目录数组
uint32_t pgd_kern[PAGE_PGD_SIZE] __attribute__((aligned(PAGE_SIZE)));

// 内核页表数组
static uint32_t pte_kern[PAGE_PTE_COUNT][PAGE_PTE_SIZE] __attribute__((aligned(PAGE_SIZE)));

// 内核物理内存池
static struct memory_pool kernel_pool;
// 用户物理内存池
static struct memory_pool user_pool;

// 内核虚拟地址池
static struct vaddr_pool kernel_vaddr_pool;

// 内核内存块描述符数组
struct mem_block_desc kernel_block_descs[MEM_BLOCK_DESC_COUNT];

// 初始化内核虚拟地址
static void init_kernel_vmm() {
	uint32_t kern_pte_base = GET_PGD_INDEX(KERNEL_OFFSET);
	uint32_t kern_pte_count = PAGE_PTE_COUNT / 2;
	// 内核页目录表最多 1023 - 768 + 1 = 256
	// 768~1023 --> 0~255
	if(kern_pte_count > 256) {
		kern_pte_count = 256;
	}
	for(uint32_t i = kern_pte_base, j = 0; i < kern_pte_count + kern_pte_base; i++, j++) {
		pgd_kern[i] = V2P((uint32_t) pte_kern[j]) | PAGE_US_U | PAGE_P_1 | PAGE_RW_W;
	}
	// 其他页目录项最多 0-767
	uint32_t other_pte_count = PAGE_PTE_COUNT - kern_pte_count;
	for(uint32_t i = 0, j = kern_pte_count; i < other_pte_count; i++, j++) {
		pgd_kern[i] = V2P((uint32_t) pte_kern[j]) | PAGE_US_U | PAGE_P_1 | PAGE_RW_W;
	}
	// 映射0-8MB内存
	uint32_t *pte = (uint32_t*) pte_kern;
	for(uint32_t i = 0; i < 2 * PAGE_PTE_SIZE; i++) {
		pte[i] = (i << 12) | PAGE_US_U | PAGE_P_1 | PAGE_RW_W;
	}
	__asm__ __volatile__("mov %0, %%cr3" : : "r"(V2P((uint32_t) pgd_kern)));
}

// 初始化物理内存管理
static void init_mem_pool(uint32_t mem_size) {
	// total memory, total pages
	uint32_t total_mem_size = mem_size - KERNEL_END_PADDR;
	uint32_t total_pages = total_mem_size / PAGE_SIZE;
	
	// -------- kernel_pool -----------
	uint32_t kernel_pages = total_pages / 2;
	uint32_t kernel_btmp_len = kernel_pages / 8;
	
	kernel_pool.paddr_start = KERNEL_END_PADDR;
	kernel_pool.pool_size = kernel_pages * PAGE_SIZE;
	kernel_pool.pool_btmp.byte_len = kernel_btmp_len;
	kernel_pool.pool_btmp.bits = (void*) P2V(MM_BITMAP_PADDR);
	
	init_bitmap(&kernel_pool.pool_btmp);
	
	lock_init(&kernel_pool.lock);
	
	// -------- kernel_vaddr_pool -------------
	kernel_vaddr_pool.vaddr_start = KERNEL_VADDR_START;
	kernel_vaddr_pool.vaddr_btmp.byte_len = kernel_btmp_len;
	kernel_vaddr_pool.vaddr_btmp.bits = (void*) P2V(MM_BITMAP_PADDR + kernel_btmp_len);
	
	init_bitmap(&kernel_vaddr_pool.vaddr_btmp);
	
	// -------- user_pool ---------------
	uint32_t user_pages = total_pages - kernel_pages;
	uint32_t user_btmp_len = user_pages / 8;
	
	user_pool.paddr_start = KERNEL_END_PADDR + kernel_pages * PAGE_SIZE;
	user_pool.pool_size = user_pages * PAGE_SIZE;
	user_pool.pool_btmp.byte_len = user_btmp_len;
	user_pool.pool_btmp.bits = (void*) P2V(MM_BITMAP_PADDR + kernel_btmp_len * 2);
	
	init_bitmap(&user_pool.pool_btmp);
	
	lock_init(&user_pool.lock);
}

// kernel space virtual address to physic address
uint32_t kern_v2p(uint32_t vaddr) {
	uint32_t pgd_index = \
		GET_PGD_INDEX(vaddr) - GET_PGD_INDEX(KERNEL_OFFSET);
	uint32_t *pte = (uint32_t*) pte_kern[pgd_index];
	uint32_t pte_index = GET_PTE_INDEX(vaddr);
	return pte[pte_index];
}

// 获取page_count个虚拟地址页(虚拟地址是连续的,可以分配多页)
static void *get_vaddr(uint32_t page_count, enum pool_flag pf) {
	ASSERT((pf == PF_KERNEL) || (pf == PF_USER));
	int v_index;
	uint32_t vaddr_start;
	if(pf == PF_KERNEL) {
		v_index = alloc_bitmap(&kernel_vaddr_pool.vaddr_btmp, page_count);
		vaddr_start = kernel_vaddr_pool.vaddr_start;
	} else if(pf == PF_USER) {
		struct task_struct *pthread = current_thread();
		v_index = alloc_bitmap(&pthread->prog_vaddr.vaddr_btmp, page_count);
		vaddr_start = pthread->prog_vaddr.vaddr_start;
	} else {
		return NULL;
	}
	if(v_index == -1) {
		return NULL;
	}
	return (void*) (vaddr_start + v_index * PAGE_SIZE);
}

// 获取一个物理地址页(物理地址可以不连续,故每次分配一页)
static void *get_paddr(enum pool_flag pf) {
	ASSERT((pf == PF_KERNEL) || (pf == PF_USER));
	struct memory_pool *mem_pool;
	if(pf == PF_KERNEL) {
		mem_pool = &kernel_pool;
	} else if(pf == PF_USER) {
		mem_pool = &user_pool;
	} else {
		return NULL;
	}
	int p_index = alloc_bitmap(&mem_pool->pool_btmp, 1);
	if(p_index == -1) {
		return NULL;
	}
	return (void*) (mem_pool->paddr_start + p_index * PAGE_SIZE);
}

// 将虚拟地址vaddr映射到物理地址paddr
static void vp_map(void *vaddr, void *paddr, enum pool_flag pf) {
	ASSERT(vaddr != NULL && paddr != NULL);
	ASSERT((pf == PF_KERNEL) || (pf == PF_USER));
	uint32_t _vaddr = (uint32_t) vaddr;
	uint32_t _paddr = (uint32_t) paddr;
	if(pf == PF_KERNEL) {
		_paddr |= (PAGE_P_1 | PAGE_RW_W);
	} else if(pf == PF_USER) {
		_paddr |= (PAGE_US_U | PAGE_P_1 | PAGE_RW_W);
	}
	uint32_t pgd_index;
	if(pf == PF_KERNEL) {
		pgd_index = GET_PGD_INDEX(_vaddr) - GET_PGD_INDEX(KERNEL_OFFSET);
	} else if(pf == PF_USER) {
		pgd_index = GET_PGD_INDEX(_vaddr) + 256;
	}
	uint32_t pte_index = GET_PTE_INDEX(_vaddr);
	pte_kern[pgd_index][pte_index] = _paddr;
}

// 解除虚拟地址vaddr和物理地址paddr的映射
static void vp_unmap(void *vaddr) {
	ASSERT(vaddr != NULL);
	uint32_t _vaddr = (uint32_t) vaddr;
	uint32_t v_index = (_vaddr - kernel_vaddr_pool.vaddr_start) / PAGE_SIZE;
	set_bitmap(&kernel_vaddr_pool.vaddr_btmp, v_index, 0);
	struct memory_pool *mem_pool;
	enum pool_flag pf;
	if(_vaddr >= KERNEL_VADDR_START) {
		mem_pool = &kernel_pool;
		pf = PF_KERNEL;
	} else if((_vaddr >= USER_VADDR_START) || (_vaddr < KERNEL_OFFSET)) {
		mem_pool = &user_pool;
		pf = PF_USER;
	}
	ASSERT((pf == PF_KERNEL) || (pf == PF_USER));
	uint32_t pgd_index;
	if(pf == PF_KERNEL) {
		pgd_index = GET_PGD_INDEX(_vaddr) - GET_PGD_INDEX(KERNEL_OFFSET);
	} else if(pf == PF_USER) {
		pgd_index = GET_PGD_INDEX(_vaddr) + 256;
	}
	uint32_t pte_index = GET_PTE_INDEX(_vaddr);
	uint32_t p_index = (pte_kern[pgd_index][pte_index] - mem_pool->paddr_start) / PAGE_SIZE;
	set_bitmap(&mem_pool->pool_btmp, p_index, 0);
	if(pf == PF_KERNEL) {
		pgd_index = GET_PGD_INDEX(_vaddr) - GET_PGD_INDEX(KERNEL_OFFSET);
	} else if(pf == PF_USER) {
		pgd_index = GET_PGD_INDEX(_vaddr) + 256;
	}
	pte_index = GET_PTE_INDEX(_vaddr);
	pte_kern[pgd_index][pte_index] &= ~(PAGE_P_1);
	// 清除TLB缓存
	__asm__ __volatile__("invlpg (%0)" : : "r"(_vaddr) : "memory");
}

// 分配size个页(4KB)的空间
void *kmalloc(uint32_t size, enum pool_flag pf) {
	ASSERT((pf == PF_KERNEL) || (pf == PF_USER));
	void *vaddr = get_vaddr(size, pf);
	if(vaddr == NULL) {
		return NULL;
	}
	uint32_t count = size;
	uint32_t _vaddr = (uint32_t) vaddr;
	if(pf == PF_KERNEL) {
		while(count-- > 0) {
			void *paddr = get_paddr(pf);
			if(paddr == NULL) {
				if(count + 1 == size) { // 未分配任何物理页
					return NULL;
				} else { // 释放掉原来已分配的内存
					uint32_t tmp_vaddr = (uint32_t) vaddr;
					for(uint32_t i = 0; i < size - (count + 1); i++) {
						vp_unmap((void*) tmp_vaddr);
						tmp_vaddr += PAGE_SIZE;
					}
					return NULL;
				}
			}
			vp_map((void*) _vaddr, paddr, pf);
			_vaddr += PAGE_SIZE;
		}
		return vaddr;
	} else if(pf == PF_USER) {
		return get_prog_pages(_vaddr, size);
	} else {
		return NULL;
	}
}

// 释放以虚拟地址vaddr为起始的size个页框
void kfree(void *vaddr, uint32_t size) {
	ASSERT(vaddr != NULL);
	uint32_t _vaddr = (uint32_t) vaddr;
	while(size-- > 0) {
		vp_unmap((void*) _vaddr);
		_vaddr += PAGE_SIZE;
	}
}

// 在内核物理内存池中申请size个物理页,并返回虚拟地址
void *get_kernel_pages(uint32_t size) {
	lock_acquire(&kernel_pool.lock);
	void *vaddr = kmalloc(size, PF_KERNEL);
	lock_release(&kernel_pool.lock);
	return vaddr;
}

// 获取进程从虚拟地址vaddr开始的size个物理页,并返回虚拟地址
void *get_prog_pages(uint32_t vaddr, uint32_t size) {
	ASSERT((vaddr >= USER_VADDR_START) || (vaddr < KERNEL_OFFSET));
	uint32_t *pgd = (uint32_t*) current_thread()->pgdir;
	uint32_t _vaddr = vaddr;
	uint32_t pgd_index;
	uint32_t pte_index;
	while(size-- > 0) {
		pgd_index = GET_PGD_INDEX(_vaddr);
		pte_index = GET_PTE_INDEX(_vaddr);
		lock_acquire(&kernel_pool.lock);
		void *tmp_vaddr = get_vaddr(1, PF_KERNEL);
		void *tmp_paddr = get_paddr(PF_KERNEL);
		vp_map(tmp_vaddr, tmp_paddr, PF_KERNEL);
		lock_release(&kernel_pool.lock);
		pgd[pgd_index] = (uint32_t) tmp_paddr | PAGE_US_U | PAGE_P_1 | PAGE_RW_W;
		((uint32_t*) tmp_vaddr)[pte_index] = \
			(uint32_t) get_paddr(PF_USER) | PAGE_US_U | PAGE_P_1 | PAGE_RW_W;
		// 解除tmp_vaddr和tmp_paddr的映射
		set_bitmap(&kernel_vaddr_pool.vaddr_btmp, 
			((uint32_t) tmp_vaddr - kernel_vaddr_pool.vaddr_start) / PAGE_SIZE, 0);
		pgd_index = GET_PGD_INDEX((uint32_t) tmp_vaddr) - GET_PGD_INDEX(KERNEL_OFFSET);
		pte_index = GET_PTE_INDEX((uint32_t) tmp_vaddr);
		pte_kern[pgd_index][pte_index] &= ~(PAGE_P_1);
		// 清除TLB缓存
		__asm__ __volatile__("invlpg (%0)" : : "r"((uint32_t) tmp_vaddr) : "memory");
		_vaddr += PAGE_SIZE;
	}
	return (void*) vaddr;
}

// 初始化内存块描述符
void init_block_desc(struct mem_block_desc *desc_arr) {
	uint16_t block_size = 16;
	for(uint8_t i = 0; i < MEM_BLOCK_DESC_COUNT; i++) {
		desc_arr[i].block_size = block_size;
		desc_arr[i].block_count = (PAGE_SIZE - sizeof(struct arena)) / block_size;
		list_init(&desc_arr[i].free_list);
		block_size *= 2;
	}
}

// 返回arena中第index个内存块的地址
static struct mem_block *arena2block(struct arena *arena, uint32_t index) {
	return (struct mem_block*) \
		((uint32_t) arena + sizeof(struct arena) + index * arena->desc->block_size);
}

// 返回内存块block所在的arena地址
static struct arena *block2arena(struct mem_block *block) {
	return (struct arena*) ((uint32_t) block & 0xfffff000);
}

// 在堆中申请size字节内存
void *sys_malloc(uint32_t size) {
	enum pool_flag pf;
	struct memory_pool *mem_pool;
	uint32_t pool_size;
	struct mem_block_desc *descs;
	struct task_struct *cur_thread = current_thread();
	// 判断用哪个内存池
	if(cur_thread->pgdir == NULL) { // 内核线程
		pf = PF_KERNEL;
		mem_pool = &kernel_pool;
		pool_size = kernel_pool.pool_size;
		descs = kernel_block_descs;
	} else { // 用户进程
		pf = PF_USER;
		mem_pool = &user_pool;
		pool_size = user_pool.pool_size;
		descs = cur_thread->prog_block_descs;
	}
	// 若申请的内存不在内存池容量范围内,直接返回NULL
	if(size <= 0 || size >= pool_size) {
		return NULL;
	}
	struct arena *arena;
	struct mem_block *block;
	lock_acquire(&mem_pool->lock);
	if(size > 1024) { // 超过最大内存块1024,则分配页框
		uint32_t page_count = DIV_ROUND_UP(size + sizeof(struct arena), PAGE_SIZE);
		arena = kmalloc(page_count, pf);
		if(arena != NULL) {
			memset(arena, 0, page_count * PAGE_SIZE); // 将分配的内存清0
			arena->desc = NULL;
			arena->count = page_count;
			arena->large = true;
			lock_release(&mem_pool->lock);
			return (void*) (arena + 1); // 跨过arena大小,把剩下的内存返回
		} else {
			lock_release(&mem_pool->lock);
			return NULL;
		}
	} else { // 小于等于1024,根据mem_block_desc分配
		uint8_t desc_index;
		for(desc_index = 0; desc_index < MEM_BLOCK_DESC_COUNT; desc_index++) {
			if(size <= descs[desc_index].block_size) {
				break;
			}
		}
		// 若mem_block_desc的free_list为空,则创建新的arena提供mem_block
		if(list_empty(&descs[desc_index].free_list)) {
			arena = kmalloc(1, pf); // 分配一个页框
			if(arena == NULL) {
				lock_release(&mem_pool->lock);
				return NULL;
			}
			memset(arena, 0, PAGE_SIZE);
			arena->desc = &descs[desc_index];
			arena->large = false;
			arena->count = descs[desc_index].block_count;
			// 拆分arena成mem_block,并添加到free_list中
			enum intr_status old_status = get_intr_status();
			disable_intr();
			for(uint32_t i = 0; i < descs[desc_index].block_count; i++) {
				block = arena2block(arena, i);
				ASSERT(!list_find(&arena->desc->free_list, &block->free_ele));
				list_append(&arena->desc->free_list, &block->free_ele);
			}
			set_intr_status(old_status);
		}
		// 开始分配内存块
		block = ELE2ENTRY(struct mem_block, free_ele, \
			list_pop(&(descs[desc_index].free_list)));
		memset(block, 0, descs[desc_index].block_size);
		arena = block2arena(block);
		--arena->count;
		lock_release(&mem_pool->lock);
		return (void*) block;
	}
}

// 回收内存ptr
void sys_free(void *ptr) {
	ASSERT(ptr != NULL);
	if(ptr != NULL) {
		enum pool_flag pf;
		struct memory_pool *mem_pool;
		if(current_thread()->pgdir == NULL) { // 内核线程
			ASSERT((uint32_t) ptr >= KERNEL_VADDR_START);
			pf = PF_KERNEL;
			mem_pool = &kernel_pool;
		} else { // 用户进程
			pf = PF_USER;
			mem_pool = &user_pool;
		}
		lock_acquire(&mem_pool->lock);
		struct mem_block *block = ptr;
		struct arena *arena = block2arena(block);
		ASSERT((arena->large == true) || (arena->large == false));
		if(arena->desc == NULL  && arena->large == true) { // 大于1024的内存
			kfree(arena, arena->count);
		} else { // 小于等于1024的内存块
			// 先将内存块回收到free_list
			list_append(&arena->desc->free_list, &block->free_ele);
			// 再判断arena中的内存块是否都是空闲,如果是则释放
			if(++arena->count == arena->desc->block_count) {
				for(uint32_t i = 0; i < arena->desc->block_count; i++) {
					struct mem_block *block_tmp = arena2block(arena, i);
					ASSERT(list_find(&arena->desc->free_list, &block_tmp->free_ele));
					list_remove(&block_tmp->free_ele);
				}
				kfree(arena, 1);
			}
		}
		lock_release(&mem_pool->lock);
	}
}

// 内存管理初始化
void mm_init() {
	init_kernel_vmm();
	init_mem_pool(*((uint32_t*) P2V(TOTAL_MEM_SIZE_PADDR)));
	init_block_desc(kernel_block_descs);
	
	printk("mm_init done\n");
}



























































#include "types.h"
#include "global.h"
#include "print.h"
#include "bitmap.h"
#include "debug.h"
#include "memory.h"
#include "sync.h"
#include "thread.h"

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

// 初始化内核虚拟地址
void init_kernel_vmm() {
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
	
	printk("init_kernel_vmm done\n");
}

// 初始化物理内存管理
void init_mem_pool(uint32_t mem_size) {
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

// 获取虚拟地址
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

// 获取物理地址
static void *get_paddr(uint32_t page_count, enum pool_flag pf) {
	ASSERT((pf == PF_KERNEL) || (pf == PF_USER));
	struct memory_pool mem_pool;
	if(pf == PF_KERNEL) {
		mem_pool = kernel_pool;
	} else if(pf == PF_USER) {
		mem_pool = user_pool;
	} else {
		return NULL;
	}
	int p_index = alloc_bitmap(&mem_pool.pool_btmp, page_count);
	if(p_index == -1) {
		return NULL;
	}
	return (void*) (mem_pool.paddr_start + p_index * PAGE_SIZE);
}

// 将虚拟地址vaddr映射到物理地址paddr,共size个页
static void vp_map(void *vaddr, void *paddr, uint32_t size, enum pool_flag pf) {
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
	uint32_t *pte;
	uint32_t pte_index;
	while(size-- > 0) {
		if(pf == PF_KERNEL) {
			pgd_index = GET_PGD_INDEX(_vaddr) - GET_PGD_INDEX(KERNEL_OFFSET);
		} else if(pf == PF_USER) {
			pgd_index = GET_PGD_INDEX(_vaddr) + 256;
		}
		pte = pte_kern[pgd_index];
		pte_index = GET_PTE_INDEX(_vaddr);
		pte[pte_index] = _paddr;
		_vaddr += PAGE_SIZE;
		_paddr += PAGE_SIZE;
	}
}

// 解除虚拟地址vaddr和物理地址paddr的映射,共size个页
static void vp_unmap(void *vaddr, uint32_t size) {
	ASSERT(vaddr != NULL);
	uint32_t _vaddr = (uint32_t) vaddr;
	uint32_t v_index = (_vaddr - kernel_vaddr_pool.vaddr_start) / PAGE_SIZE;
	for(uint32_t i = v_index; i < v_index + size; i++) {
		set_bitmap(&kernel_vaddr_pool.vaddr_btmp, i, 0);
	}
	struct memory_pool mem_pool;
	enum pool_flag pf;
	if(_vaddr >= KERNEL_VADDR_START) {
		mem_pool = kernel_pool;
		pf = PF_KERNEL;
	} else if((_vaddr >= USER_VADDR_START) || (_vaddr < KERNEL_OFFSET)) {
		mem_pool = user_pool;
		pf = PF_USER;
	}
	ASSERT((pf == PF_KERNEL) || (pf == PF_USER));
	uint32_t pgd_index;
	if(pf == PF_KERNEL) {
		pgd_index = GET_PGD_INDEX(_vaddr) - GET_PGD_INDEX(KERNEL_OFFSET);
	} else if(pf == PF_USER) {
		pgd_index = GET_PGD_INDEX(_vaddr) + 256;
	}
	uint32_t *pte = pte_kern[pgd_index];
	uint32_t pte_index = GET_PTE_INDEX(_vaddr);
	uint32_t p_index = (pte[pte_index] - mem_pool.paddr_start) / PAGE_SIZE;
	for(uint32_t i = p_index; i < p_index + size; i++) {
		set_bitmap(&mem_pool.pool_btmp, i, 0);
	}
	while(size-- > 0) {
		if(pf == PF_KERNEL) {
		pgd_index = GET_PGD_INDEX(_vaddr) - GET_PGD_INDEX(KERNEL_OFFSET);
		} else if(pf == PF_USER) {
			pgd_index = GET_PGD_INDEX(_vaddr) + 256;
		}
		pte = pte_kern[pgd_index];
		pte_index = GET_PTE_INDEX(_vaddr);
		pte[pte_index] &= ~(PAGE_P_1);
		// 清除TLB缓存
		__asm__ __volatile__("invlpg (%0)" : : "r"(_vaddr) : "memory");
		_vaddr += PAGE_SIZE;
	}
}

// 分配size个页(4KB)的空间
void *kmalloc(uint32_t size, enum pool_flag pf) {
	ASSERT((pf == PF_KERNEL) || (pf == PF_USER));
	void *vaddr = get_vaddr(size, pf);
	if(vaddr == NULL) {
		return NULL;
	}
	void *paddr = get_paddr(size, pf);
	if(paddr == NULL) {
		return NULL;
	}
	vp_map(vaddr, paddr, size, pf);
	return vaddr;
}

// 释放以虚拟地址vaddr为起始的size个页框
void kfree(void *vaddr, uint32_t size) {
	ASSERT(vaddr != NULL);
	vp_unmap(vaddr, size);
}

// 在用户物理内存池中申请size个物理页,并返回虚拟地址
void *get_user_pages(uint32_t size) {
	lock_acquire(&user_pool.lock);
	void *vaddr = kmalloc(size, PF_USER);
	lock_release(&user_pool.lock);
	return vaddr;
}

// 在内核物理内存池中申请size个物理页,并返回虚拟地址
void *get_kernel_pages(uint32_t size) {
	lock_acquire(&kernel_pool.lock);
	void *vaddr = kmalloc(size, PF_KERNEL);
	lock_release(&kernel_pool.lock);
	return vaddr;
}

// 获取从虚拟地址vaddr开始的size个物理页,并返回虚拟地址
void *get_pages(uint32_t vaddr, uint32_t size) {
	enum pool_flag pf;
	if(vaddr >= KERNEL_VADDR_START) {
		pf = PF_KERNEL;
	} else if((vaddr >= USER_VADDR_START) || (vaddr < KERNEL_OFFSET)) {
		pf = PF_USER;
	} else {
		return NULL;
	}
	uint32_t *pgd = (uint32_t*) current_thread()->pgdir;
	uint32_t _vaddr = vaddr;
	uint32_t pgd_index;
	uint32_t pte_index;
	while(size-- > 0) {
		pgd_index = GET_PGD_INDEX(_vaddr);
		pte_index = GET_PTE_INDEX(_vaddr);
		lock_acquire(&kernel_pool.lock);
		void *tmp_vaddr = get_vaddr(1, PF_KERNEL);
		void *tmp_paddr = get_paddr(1, PF_KERNEL);
		vp_map(tmp_vaddr, tmp_paddr, 1, PF_KERNEL);
		lock_release(&kernel_pool.lock);
		pgd[pgd_index] = (uint32_t) tmp_paddr | PAGE_US_U | PAGE_P_1 | PAGE_RW_W;
		((uint32_t*) tmp_vaddr)[pte_index] = \
			(uint32_t) get_paddr(1, pf) | PAGE_US_U | PAGE_P_1 | PAGE_RW_W;
		set_bitmap(&kernel_vaddr_pool.vaddr_btmp, 
			((uint32_t) tmp_vaddr - kernel_vaddr_pool.vaddr_start) / PAGE_SIZE, 0);
		_vaddr += PAGE_SIZE;
	}
	return (void*) vaddr;
}































































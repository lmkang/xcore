#include "stdint.h"
#include "global.h"
#include "memory.h"
#include "print.h"
#include "bitmap.h"
#include "string.h"
#include "debug.h"
#include "sync.h"
#include "sync.h"
#include "interrupt.h"

#define PAGE_SIZE 4096 // 页大小4KB

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

// 0xc009f000是内核主线程栈顶,0xc009e000是内核主线程的PCB
// 一个页框大小的位图可表示128MB内存,位图位置安排在地址0xc009a000
// 这样本系统最大支持4个页框的位图,即512MB
#define MEMORY_BITMAP_BASE 0xc009a000

// 0xc0000000是内核从虚拟地址3GB起
// 0x100000是为了跨过低端1MB内存,是虚拟地址在逻辑上连续
#define KERNEL_HEAP_START 0xc0100000

// 物理内存池结构
struct memory_pool {
	struct bitmap pool_btmp; // 本内存池用到的位图结构
	uint32_t paddr_start; // 本内存池所管理物理内存的起始地址
	uint32_t pool_size; // 本内存池字节容量
	struct lock lock; // 申请内存时互斥
};

// 内存仓库
struct arena {
	struct mem_block_desc *desc; // arena关联的mem_block_desc
	// large为true时,count表示的页框数
	// 否则count表示空闲mem_block数量
	uint32_t count;
	bool large;
};

// 内核内存块描述符数组
struct mem_block_desc kernel_block_desc[BLOCK_DESC_COUNT];

// 生成内核内存池和用户内存池(全局变量,定义在kernel/memory.h)
struct memory_pool kernel_pool, user_pool;

// 内核虚拟地址池
struct virtual_addr kernel_vaddr;

// 为malloc做准备
void init_block_desc(struct mem_block_desc *desc_array) {
	uint16_t block_size = 16;
	// 初始化每个mem_block_desc
	for(uint16_t i = 0; i < BLOCK_DESC_COUNT; i++) {
		desc_array[i].block_size = block_size;
		desc_array[i].block_count = (PAGE_SIZE - sizeof(struct arena)) / block_size;
		list_init(&desc_array[i].free_list);
		block_size *= 2;
	}
}

// 初始化内存池
static void init_mem_pool(uint32_t all_mem) {
	put_str("init_mem_pool start\n");
	// 页表数目 = 第0和第768 + 769~1022 = 256个
	uint32_t page_table_size = PAGE_SIZE * 256;
	uint32_t used_mem = page_table_size + 0x100000;
	uint32_t free_mem = all_mem - used_mem;
	uint16_t all_free_pages = free_mem / PAGE_SIZE; // 总空闲页
	uint16_t kernel_free_pages = all_free_pages / 2; // 内核空闲页
	uint16_t user_free_pages = all_free_pages - kernel_free_pages; // 用户空闲页
	
	// 为简化位图操作,余数不处理,但是这样会丢内存
	// 好处是不用做内存的越界检查,因为位图表示的内存少于实际物理内存
	uint32_t kernel_btmp_len = kernel_free_pages / 8;
	uint32_t user_btmp_len = user_free_pages / 8;
	uint32_t kernel_paddr_start = used_mem;
	uint32_t user_paddr_start = kernel_paddr_start + kernel_free_pages * PAGE_SIZE;
	
	// 内核内存池的位图定在MEMORY_BITMAP_BASE(0xc009a000)
	kernel_pool.paddr_start = kernel_paddr_start;
	kernel_pool.pool_size = kernel_free_pages * PAGE_SIZE;
	kernel_pool.pool_btmp.byte_len = kernel_btmp_len;
	kernel_pool.pool_btmp.bits = (void*) MEMORY_BITMAP_BASE;
	
	// 用户内存池的位图紧跟在内核内存池位图之后
	user_pool.paddr_start = user_paddr_start;
	user_pool.pool_size = user_free_pages * PAGE_SIZE;
	user_pool.pool_btmp.byte_len = user_btmp_len;
	user_pool.pool_btmp.bits = (void*) (MEMORY_BITMAP_BASE + kernel_btmp_len);
	
	put_str("kernel_pool_bitmap_start: ");
	put_int((int) kernel_pool.pool_btmp.bits);
	put_str("\n");
	put_str("kernel_pool_paddr_start: ");
	put_int(kernel_pool.paddr_start);
	put_str("\n");
	put_str("user_pool_bitmap_start: ");
	put_int((int) user_pool.pool_btmp.bits);
	put_str("\n");
	put_str("user_pool_paddr_start: ");
	put_int(user_pool.paddr_start);
	put_str("\n");
	
	// 将位图置0
	init_bitmap(&kernel_pool.pool_btmp);
	init_bitmap(&user_pool.pool_btmp);
	
	lock_init(&kernel_pool.lock);
	lock_init(&user_pool.lock);
	
	// 初始化内核虚拟地址的位图,按实际物理内存大小生成数组
	// 用于维护内核堆的虚拟地址,所以要和内核内存池大小一致
	kernel_vaddr.vaddr_btmp.byte_len = kernel_btmp_len;
	// 位图的数组指向一块未使用的内存,目前定位在内核内存池和用户内存池之外
	kernel_vaddr.vaddr_btmp.bits = (void*) (MEMORY_BITMAP_BASE + kernel_btmp_len + user_btmp_len);
	
	kernel_vaddr.vaddr_start = KERNEL_HEAP_START;
	init_bitmap(&kernel_vaddr.vaddr_btmp);
	
	put_str("init_mem_pool done\n");
}

// 内存管理部分初始化入口
void init_memory() {
	put_str("init_memory start\n");
	uint32_t mem_total_bytes = (*(uint32_t*) (0xb00)); // 之前获取的物理内存容量发在此处
	init_mem_pool(mem_total_bytes);
	init_block_desc(kernel_block_desc);
	put_str("init_memory done\n");
}

// 在pf表示的虚拟内存池中申请page_count个虚拟页
// 成功返回虚拟页的起始地址,失败返回NULL
static void * get_vaddr(enum pool_flag pf, uint32_t page_count) {
	int vaddr_start = 0;
	int bit_idx_start = -1;
	uint32_t count = 0;
	if(pf == PF_KERNEL) { // 内核内存池
		bit_idx_start = alloc_bitmap(&kernel_vaddr.vaddr_btmp, page_count);
		if(bit_idx_start == -1) {
			return NULL;
		}
		while(count < page_count) {
			set_bitmap(&kernel_vaddr.vaddr_btmp, bit_idx_start + count++, 1);
		}
		vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PAGE_SIZE;
	} else {
		// 用户内存池
		struct task_struct *cur_thread = current_thread();
		bit_idx_start = alloc_bitmap(&cur_thread->userprog_vaddr.vaddr_btmp, page_count);
		if(bit_idx_start == -1) {
			return NULL;
		}
		while(count < page_count) {
			set_bitmap(&cur_thread->userprog_vaddr.vaddr_btmp, bit_idx_start + (count++), 1);
		}
		vaddr_start = cur_thread->userprog_vaddr.vaddr_start + bit_idx_start * PAGE_SIZE;
		// (0xc0000000 - PAGE_SIZE)作为用户3级栈已经在start_process被分配
		ASSERT((uint32_t) vaddr_start < (0xc0000000 - PAGE_SIZE));
	}
	return (void*)vaddr_start;
}

// 得到虚拟地址vaddr对应的pte指针
uint32_t * get_pte_ptr(uint32_t vaddr) {
	// 先访问到页表自己
	// 再用页目录项pde(页目录内页表的索引)作为pte的索引访问到页表
	// 再用pte的索引作为页内偏移
	uint32_t *pte = (uint32_t*) ((0xffc00000 + ((vaddr & 0xffc00000) >> 10)) + PTE_IDX(vaddr) * 4);
	return pte;
}

// 得到虚拟地址vaddr对应的pde指针
uint32_t * get_pde_ptr(uint32_t vaddr) {
	// 0xfffff用来访问到页表本身所在的地址
	uint32_t *pde = (uint32_t*) (0xfffff000 + PDE_IDX(vaddr) * 4);
	return pde;
}

// 在mem_pool指向的物理内存池中分配1个物理页
// 成功返回页框的物理地址,失败返回NULL
static void * palloc(struct memory_pool *mem_pool) {
	// 分配或设置位图要保证原子操作
	int bit_idx = alloc_bitmap(&mem_pool->pool_btmp, 1); // 分配一个物理页
	if(bit_idx == -1) {
		return NULL;
	}
	set_bitmap(&mem_pool->pool_btmp, bit_idx, 1); // 将此位bit_idx置1
	uint32_t page_paddr = bit_idx * PAGE_SIZE + mem_pool->paddr_start;
	return (void*) page_paddr;
}

// 页表中添加虚拟地址vaddr与物理地址page_paddr的映射
static void add_page_table(void *vaddr, void *page_paddr) {
	uint32_t _vaddr = (uint32_t) vaddr;
	uint32_t _page_paddr = (uint32_t) page_paddr;
	uint32_t *pde = get_pde_ptr(_vaddr);
	uint32_t *pte = get_pte_ptr(_vaddr);
	// 注意:执行*pte时,会访问到空的pde,所以确保pde创建完成后再执行*pte
	// 否则会引发page_fault
	// 先在页目录内判断目录项的P位,若为1,则表示该表已存在
	if(*pde & 0x00000001) {
		// 页目录项和页表项的第0位为P,此处判断目录项是否存在
		ASSERT(!(*pte & 0x00000001));
		// 只要是创建页表,pte就应该不存在
		if(!(*pte & 0x00000001)) {
			*pte = (_page_paddr | PG_US_U | PG_RW_W | PG_P_1); // US=1,RW=1,P=1
		} else { // 目前应该不会执行到这里,因为ASSERT会先执行
			PANIC("pte repeat");
			*pte = (_page_paddr | PG_US_U | PG_RW_W | PG_P_1); // US=1,RW=1,P=1
		}
	} else { // 页目录项不存在,所以要先创建页目录再创建页表项
		// 页表中用到的页框一律从内核空间分配
		uint32_t pde_paddr = (uint32_t) palloc(&kernel_pool);
		*pde = (pde_paddr | PG_US_U | PG_RW_W | PG_P_1);
		// 分配到的物理页地址pde_paddr对应的物理内存清0
		// 避免里面的陈旧数据变成了页表项,从而使页表混乱
		// 访问到pde对应的物理地址,用pte取高20位即可
		// 因为pte基于该pde对应的物理地址内再寻址
		// 把低12位置为0就是该pde对应的物理页的起始
		memset((void*) ((int) pte & 0xfffff000), 0, PAGE_SIZE);
		ASSERT(!(*pte & 0x00000001));
		*pte = (_page_paddr | PG_US_U | PG_RW_W | PG_P_1); // US=1,RW=1,P=1
	}
}

// 分配page_count个页空间,成功返回起始虚拟地址,失败返回NULL
void * malloc_page(enum pool_flag pf, uint32_t page_count) {
	ASSERT(page_count > 0 && page_count < 3840);
	// malloc_page的原理是三个动作的合成
	// 1 通过get_vaddr()在虚拟内存池中申请虚拟地址
	// 2 通过palloc()在物理内存池中申请物理页
	// 3 通过add_page_table()将以上得到的虚拟地址和物理地址在页表中完成映射
	void *vaddr_start = get_vaddr(pf, page_count);
	if(vaddr_start == NULL) {
		return NULL;
	}
	uint32_t vaddr = (uint32_t) vaddr_start;
	uint32_t count = page_count;
	struct memory_pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
	// 因为虚拟地址是连续的,但物理地址可以不连续,所以逐个映射
	while(count-- > 0) {
		void *page_paddr = palloc(mem_pool);
		if(page_paddr == NULL) {
			// 失败时要将已申请的虚拟地址和物理页全部回滚
			// 将来在完成内存回收时再补充
			return NULL;
		}
		add_page_table((void*) vaddr, page_paddr);
		vaddr += PAGE_SIZE; // 下一个虚拟页
	}
	return vaddr_start;
}

// 从内核物理内存池中申请page_count页内存
// 成功返回虚拟地址,失败返回NULL
void * alloc_kernel_page(uint32_t page_count) {
	lock_acquire(&kernel_pool.lock);
	void *vaddr = malloc_page(PF_KERNEL, page_count);
	if(vaddr != NULL) { // 若分配的地址不为空,将页框清0后返回
		memset(vaddr, 0, page_count * PAGE_SIZE);
	}
	lock_release(&kernel_pool.lock);
	return vaddr;
}

// 从用户空间中申请page_count页内存
// 成功返回虚拟地址,失败返回NULL
void * alloc_user_page(uint32_t page_count) {
	lock_acquire(&user_pool.lock);
	void *vaddr = malloc_page(PF_USER, page_count);
	memset(vaddr, 0, page_count * PAGE_SIZE);
	lock_release(&user_pool.lock);
	return vaddr;
}

// 将地址vaddr与pf对应的池中的物理地址相关联,仅支持一页空间分配
void * get_a_page(enum pool_flag pf, uint32_t vaddr) {
	struct memory_pool *mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
	lock_acquire(&mem_pool->lock);
	// 先将虚拟地址对应的位图置1
	struct task_struct *cur_thread = current_thread();
	int32_t bit_idx = -1;
	// 若当前是用户进程申请用户内存,就修改用户进程的虚拟地址位图
	if(cur_thread->page_vaddr != NULL && pf == PF_USER) {
		bit_idx = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PAGE_SIZE;
		ASSERT(bit_idx > 0);
		set_bitmap(&cur_thread->userprog_vaddr.vaddr_btmp, bit_idx, 1);
	} else if(cur_thread->page_vaddr == NULL && pf == PF_KERNEL) {
		// 若是内核线程申请内核内存,就修改kernel_vaddr
		bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PAGE_SIZE;
		ASSERT(bit_idx > 0);
		set_bitmap(&kernel_vaddr.vaddr_btmp, bit_idx, 1);
	} else {
		PANIC("get_a_page: not allow kernel alloc user space or user alloc kernel space");
	}
	void *page_paddr = palloc(mem_pool);
	if(page_paddr == NULL) {
		return NULL;
	}
	add_page_table((void*) vaddr, page_paddr);
	lock_release(&mem_pool->lock);
	return (void*) vaddr;
}

// 得到虚拟地址映射到的物理地址
uint32_t addr_v2p(uint32_t vaddr) {
	uint32_t *pte = get_pte_ptr(vaddr);
	// (*pte)的值是页表所在的物理页框地址
	// 去掉其低12位的页表项属性+虚拟地址vaddr的低12位
	return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

// 返回arena中第idx个内存块的地址
static struct mem_block * arena2block(struct arena *a, uint32_t idx) {
	return (struct mem_block*) ((uint32_t) a + sizeof(struct arena) + idx * a->desc->block_size);
}

// 返回内存块block所在的arena地址
static struct arena * block2arena(struct mem_block *block) {
	return (struct arena*) ((uint32_t) block & 0xfffff000);
}

// 在堆中申请size字节内存
void * sys_malloc(uint32_t size) {
	enum pool_flag pf;
	struct memory_pool *mem_pool;
	uint32_t pool_size;
	struct mem_block_desc *descs;
	struct task_struct *cur_thread = current_thread();
	// 判断是哪个内存池
	if(cur_thread->page_vaddr == NULL) { // 内核线程
		pf = PF_KERNEL;
		pool_size = kernel_pool.pool_size;
		mem_pool = &kernel_pool;
		descs = kernel_block_desc;
	} else { // 用户进程,page_vaddr在分配页表时创建
		pf = PF_USER;
		pool_size = user_pool.pool_size;
		mem_pool = &user_pool;
		descs = cur_thread->user_block_desc;
	}
	// 若申请的内存不在内存池容量范围内,返回NULL
	if(size <= 0 || size >= pool_size) {
		return NULL;
	}
	struct arena *a;
	struct mem_block *block;
	lock_acquire(&mem_pool->lock);
	// 超过最大内存块1024,就分配页框
	if(size > 1024) {
		// 向上取整
		uint32_t page_count = DIV_ROUND_UP(size + sizeof(struct arena), PAGE_SIZE);
		a = malloc_page(pf, page_count);
		if(a != NULL) {
			memset(a, 0, page_count * PAGE_SIZE); // 将分配的内存清0
			// 对于分配的大块页框,desc=NULL,count=page_count,large=true
			a->desc = NULL;
			a->count = page_count;
			a->large = true;
			lock_release(&mem_pool->lock);
			return (void*) (a + 1); // 跨过arena大小,把剩下的内存返回
		} else {
			lock_release(&mem_pool->lock);
			return NULL;
		}
	} else { // 申请的内存 <= 1024
		uint8_t desc_idx;
		for(desc_idx = 0; desc_idx < BLOCK_DESC_COUNT; desc_idx++) {
			if(size <= descs[desc_idx].block_size) {
				// 从小往大找,找到后退出
				break;
			}
		}
		// 若mem_block_desc的free_list中已经没有可用的mem_block
		// 则创建新的arena提供mem_block
		if(list_empty(&descs[desc_idx].free_list)) {
			a = malloc_page(pf, 1); // 分配1个页框作为arena
			if(a == NULL) {
				lock_release(&mem_pool->lock);
				return NULL;
			}
			memset(a, 0, PAGE_SIZE);
			// 对于分配的小块内存,将desc置为相应内存块描述符
			// count置为arena可用的内存块数,large置为false
			a->desc = &descs[desc_idx];
			a->count = descs[desc_idx].block_count;
			a->large = false;
			enum intr_status old_status = get_intr_status();
			disable_intr();
			// 将arena拆分成内存块,并添加到内存块描述符的free_list中
			for(uint8_t block_idx = 0; block_idx < descs[desc_idx].block_count; block_idx++) {
				block = arena2block(a, block_idx);
				ASSERT(!has_ele(&a->desc->free_list, &block->free_ele));
				list_append(&a->desc->free_list, &block->free_ele);
			}
			set_intr_status(old_status);
		}
		// 开始分配内存块
		block = ele2entry(struct mem_block, free_ele, list_pop(&(descs[desc_idx].free_list)));
		memset(block, 0, descs[desc_idx].block_size);
		a = block2arena(block); // 获取内存块block所在的arena
		--a->count; // 将此arena中的空闲内存块数减1
		lock_release(&mem_pool->lock);
		return (void*) block;
	}
}

// 将物理地址page_paddr回收到物理内存池
void pfree(uint32_t page_paddr) {
	struct memory_pool *mem_pool;
	uint32_t bit_idx = 0;
	if(page_paddr >= user_pool.paddr_start) { // 用户物理内存池
		mem_pool = &user_pool;
	} else { // 内核物理内存池
		mem_pool = &kernel_pool;
	}
	bit_idx = (page_paddr - mem_pool->paddr_start) / PAGE_SIZE;
	set_bitmap(&mem_pool->pool_btmp, bit_idx, 0); // 将位图中该位清0
}

// 去掉页表中虚拟地址vaddr的映射,只去掉vaddr对应的pte
static void remove_page_table_pte(uint32_t vaddr) {
	uint32_t *pte = get_pte_ptr(vaddr);
	*pte &= ~PG_P_1; // 将页表项pte的P位置0
	__asm__ __volatile__("invlpg %0" : : "m"(vaddr) : "memory"); // 更新TLB
}

// 在虚拟地址池中释放以vaddr起始的连续page_count个虚拟页地址
static void remove_vaddr(enum pool_flag pf, void *vaddr, uint32_t page_count) {
	uint32_t bit_idx_start = 0;
	uint32_t _vaddr = (uint32_t) vaddr;
	uint32_t count = 0;
	if(pf == PF_KERNEL) { // 内核虚拟内存池
		bit_idx_start = (_vaddr - kernel_vaddr.vaddr_start) / PAGE_SIZE;
		while(count < page_count) {
			set_bitmap(&kernel_vaddr.vaddr_btmp, bit_idx_start + (count++), 0);
		}
	} else { // 用户虚拟内存池
		struct task_struct *cur_thread = current_thread();
		bit_idx_start = (_vaddr - cur_thread->userprog_vaddr.vaddr_start) / PAGE_SIZE;
		while(count < page_count) {
			set_bitmap(&cur_thread->userprog_vaddr.vaddr_btmp, bit_idx_start + (count++), 0);
		}
	}
}

// 释放以虚拟地址vaddr为起始的page_count个物理页框
void mfree_page(enum pool_flag pf, void *vaddr, uint32_t page_count) {
	uint32_t page_paddr;
	uint32_t _vaddr = (uint32_t) vaddr;
	uint32_t count = 0;
	ASSERT(page_count >= 1 && _vaddr % PAGE_SIZE == 0);
	page_paddr = addr_v2p(_vaddr); // 虚拟地址->物理地址
	
	// 确保待释放的物理内存在低端1MB + 1KB大小的页目录+1KB的页表地址范围外
	ASSERT((page_paddr % PAGE_SIZE) == 0 && page_paddr >= 0x102000);
	
	if(page_paddr >= user_pool.paddr_start) { // 用户物理内存池
		while(count < page_count) {
			page_paddr = addr_v2p(_vaddr);
			// 确保物理地址属于用户物理内存池
			ASSERT((page_paddr % PAGE_SIZE) == 0 && page_paddr >= user_pool.paddr_start);
			// 先将对应的物理页框归还到内存池
			pfree(page_paddr);
			// 再从页表中清除此虚拟地址所在的页表项pte
			remove_page_table_pte(_vaddr);
			_vaddr += PAGE_SIZE;
			++count;
		}
		// 清空虚拟地址的位图中的相应位
		remove_vaddr(pf, vaddr, page_count);
	} else { // 内核物理内存池
		while(count < page_count) {
			page_paddr = addr_v2p(_vaddr);
			// 确保待释放的物理内存只属于内核物理内存池
			ASSERT((page_paddr % PAGE_SIZE) == 0 && page_paddr >= kernel_pool.paddr_start 
				&& page_paddr < user_pool.paddr_start);
			// 先将对应的物理页框归还到内存池
			pfree(page_paddr);
			// 再从页表中清除此虚拟地址所在的页表项pte
			remove_page_table_pte(_vaddr);
			_vaddr += PAGE_SIZE;
			++count;
		}
		// 清空虚拟地址的位图中德相应位
		remove_vaddr(pf, vaddr, page_count);
	}
}

// 回收内存ptr
void sys_free(void *ptr) {
	ASSERT(ptr != NULL);
	if(ptr != NULL) {
		enum pool_flag pf;
		struct memory_pool *mem_pool;
		// 判断是线程,还是进程
		if(current_thread()->page_vaddr == NULL) { // 内核线程
			ASSERT((uint32_t) ptr >= KERNEL_HEAP_START);
			pf = PF_KERNEL;
			mem_pool = &kernel_pool;
		} else { // 用户进程
			pf = PF_USER;
			mem_pool = &user_pool;
		}
		lock_acquire(&mem_pool->lock);
		struct mem_block *block = ptr;
		struct arena *a = block2arena(block);
		ASSERT(a->large == true || a->large == false);
		if(a->desc == NULL && a->large == true) { // 大于1024的内存
			mfree_page(pf, a, a->count);
		} else { // 小于等于1024的内存块
			// 先将内存块回收到free_list
			list_append(&a->desc->free_list, &block->free_ele);
			// 判断此arena中的内存块是否都是空闲,决定是否释放arena
			if(++a->count == a->desc->block_count) {
				for(uint32_t block_idx = 0; block_idx < a->desc->block_count; block_idx++) {
					struct mem_block *b = arena2block(a, block_idx);
					ASSERT(has_ele(&a->desc->free_list, &b->free_ele));
					list_remove(&b->free_ele);
				}
				mfree_page(pf, a, 1);
			}
		}
		lock_release(&mem_pool->lock);
	}
}
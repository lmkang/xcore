#include "stdint.h"
#include "global.h"
#include "memory.h"
#include "print.h"
#include "bitmap.h"
#include "string.h"
#include "debug.h"
#include "sync.h"
#include "sync.h"

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

// 生成内核内存池和用户内存池(全局变量,定义在kernel/memory.h)
struct memory_pool kernel_pool, user_pool;

// 内核虚拟地址池
struct virtual_addr kernel_vaddr;

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
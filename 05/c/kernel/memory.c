#include "stdint.h"
#include "memory.h"
#include "print.h"
#include "bitmap.h"

#define PAGE_SIZE 4096

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
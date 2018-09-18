	%include "loader.inc"
	%include "boot.inc"
	
	org LOADER_BASE_ADDR
	
	LOADER_STACK_TOP equ LOADER_BASE_ADDR
	
GDT_BASE:
	dd 0x00000000
	dd 0x00000000
	
CODE_DESC:
	SEG_ASM 0x08, 0x0000, 0xffffffff
DATA_DESC:
	SEG_ASM 0x02, 0x0000, 0xffffffff
VIDEO_DESC:
	SEG_ASM 0x02, 0xb8000, 0x00007fff
	
	GDT_SIZE equ $ - GDT_BASE
	GDT_LIMIT equ GDT_SIZE - 1
	
	;预留60个段描述符
	times 60 dq 0
	
	SELECTOR_CODE equ (0x0001 << 3) + TI_GDT + RPL0
	SELECTOR_DATA equ (0x0002 << 3) + TI_GDT + RPL0
	SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0
	
	;全部物理内存的字节数
	;此处的物理地址是:0x900+4*8+60*8=0x900+0x200=0xb00
	total_mem_bytes dd 0
	
GDT_PTR:
	dw GDT_LIMIT
	dd GDT_BASE
	
	;此处为了loader_start能对齐内存:
	;0xb00+total_mem_bytes+GDT_PTR+ards_buf+ards_nr
	;=0xb00+4+6+244+2=0xb00+256=0xc00
	
	;ards结构体
	ards_buf times 244 db 0
	
	;ards结构体数量
	ards_nr dw 0
	
loader_start:
	;开始尝试获取物理内存容量
	;初始化:ebx=0,edx=0x534d4150(SMAP的ASCII码)
	;di指向ARDS结构缓冲区ards_buf
	xor ebx, ebx
	mov edx, 0x534d4150
	mov di, ards_buf
;1、EAX=0xe820
try_e820_mem:
	mov eax, 0x0000e820
	mov ecx, 20
	int 0x15
	jc try_e801_mem
	;di指向缓冲区的下一个ARDS结构
	add di, cx
	;增加ARDS结构数量
	inc word [ards_nr]
	cmp ebx, 0
	jnz try_e820_mem
	mov cx, [ards_nr]
	mov ebx, ards_buf
	xor edx, edx
;冒泡排序,找出最大内存块,放在edx
;不需要判断type,最大内存块一定是可用的
.find_max_mem:
	mov eax, [ebx]
	add eax, [ebx + 8]
	add ebx, 20
	cmp edx, eax
	jge .next_ards
	mov edx, eax
.next_ards:
	loop .find_max_mem
	jmp mem_get_ok
;2、AX=0xe801
try_e801_mem:
	mov ax, 0xe801
	int 0x15
	jc try_88_mem
	;先计算15MB的内存
	;ax和cx是以KB为单位的内存数量,将其转为Byte单位
	mov cx, 0x400
	mul cx
	shl edx, 16
	and eax, 0x0000ffff
	or edx, eax
	add edx, 0x100000
	mov esi, edx
	;再计算16MB以上的内存
	;bx和dx是以64KB为单位的内存数量,将其转为Byte单位
	xor eax, eax
	mov ax, bx
	;32乘法,被乘数是eax,积为64位
	;积的高32位在edx中,低32位在eax中
	mov ecx, 0x10000
	mul ecx
	;此方法只能测出4GB以内的内存,32位的eax足够了
	;edx肯定为0,只需要加eax就行了
	add esi, eax
	mov edx, esi
	jmp mem_get_ok
;3、AH=0x88
try_88_mem:
	mov ah, 0x88
	int 0x15
	jc mem_get_error
	;ax存储的是以KB为单位的内存容量
	and eax, 0x0000ffff
	;16位乘法,被乘数是ax,积为32位
	;积的高16位在dx中,低16位在ax中
	mov cx, 0x400
	mul cx
	shl edx, 16
	or edx, eax
	;0x88只会返回1MB以上的内存,故加上1MB
	add edx, 0x100000
	jmp mem_get_ok
mem_get_error:
	hlt
	jmp mem_get_error
mem_get_ok
	mov [total_mem_bytes], edx

	;1、打开A20Gate
	in al, 0x92
	or al, 0x02
	out 0x92, al
	
	;2、加载GDT
	lgdt [GDT_PTR]
	
	;3、PE位设为1
	mov eax, cr0
	or eax, 0x01
	mov cr0, eax
	
	;4、jmp清空流水线
	jmp dword SELECTOR_CODE:p_mode_start
[bits 32]
p_mode_start:
	mov ax, SELECTOR_DATA
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov esp, LOADER_STACK_TOP
	mov ax, SELECTOR_VIDEO
	mov gs, ax
	
	;将kernel从硬盘读入内存
	mov eax, KERNEL_START_SECTOR
	mov ebx, KERNEL_BIN_ADDR
	mov ecx, KERNEL_SECTOR_COUNT
	call read_disk_m32
	
	;setup page
	call setup_page
	
	;save gdt
	sgdt [GDT_PTR]
	mov ebx, [GDT_PTR + 2]
	;video_desc在第三个段描述符,故3 * 8 = 0x18
	;段基址在高4字节
	or dword [ebx + 0x18 + 4], 0xc0000000
	;change gdt to base on 0xc0000000
	add dword [GDT_PTR + 2], 0xc0000000
	
	;update stack top
	add esp, 0xc0000000
	
	;open page
	mov eax, PAGE_DIR_TABLE_ADDR
	mov cr3, eax
	mov eax, cr0
	or eax, 0x80000000
	mov cr0, eax
	lgdt [GDT_PTR]
enter_kernel:
	call parse_kernel
	mov esp, KERNEL_STACK_ADDR ;stack top
	jmp KERNEL_ENTRY_ADDR

;setup page
;setup page directory and page table
setup_page:
	;把页目录占用的空间逐字节清0
	mov ecx, 4096
	mov esi, 0
.clear_page_dir:
	mov byte [PAGE_DIR_TABLE_ADDR + esi], 0
	inc esi
	loop .clear_page_dir
;创建页目录项(PDE)
.create_pde:
	mov eax, PAGE_DIR_TABLE_ADDR
	;此时,eax为第一个页表的位置及属性
	add eax, 0x1000
	;为.create_pte做准备,ebx为基址
	mov ebx, eax
	;将0xc03fffff以下的地址和0x003fffff以下的地址都指向相同的页表
	or eax, PG_US_U | PG_RW_W | PG_P
	mov [PAGE_DIR_TABLE_ADDR + 0x0], eax
	mov [PAGE_DIR_TABLE_ADDR + 0xc00], eax
	;使最后一个页目录项指向页目录表自身的地址
	sub eax, 0x1000
	mov [PAGE_DIR_TABLE_ADDR + 4092], eax
	
	;开始创建页表项(PTE)
	;1MB低端内存 / 每页大小4KB = 256
	mov ecx, 256
	mov esi, 0
	mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
	;ebx在上面已经被赋值,ebx=0x101000
	mov [ebx + esi * 4], edx
	add edx, 4096
	inc esi
	loop .create_pte
	
	;创建内核其他页表的PDE
	;此时eax为第二个页表的位置
	mov eax, PAGE_DIR_TABLE_ADDR
	add eax, 0x2000
	or eax, PG_US_U | PG_RW_W | PG_P
	mov ebx, PAGE_DIR_TABLE_ADDR
	;范围是769~1022的所有页目录项
	mov ecx, 254
	mov esi, 769
.create_kernel_pde:
	mov [ebx + esi * 4], eax
	inc esi
	add eax, 0x1000
	loop .create_kernel_pde
	ret

;解析内核文件(ELF)
parse_kernel:
	xor eax, eax
	xor ebx, ebx ;ebx记录程序头表地址
	xor ecx, ecx ;cx记录程序头表中的program header数量,即e_phnum
	xor edx, edx ;dx记录program header尺寸,即e_phentsize
	
	mov dx, [KERNEL_BIN_ADDR + 42] ;e_phentsize
	mov ebx, [KERNEL_BIN_ADDR + 28] ;e_phoff
	
	add ebx, KERNEL_BIN_ADDR
	mov cx, [KERNEL_BIN_ADDR + 44] ;e_phnum
.each_segment:
	;若p_type等于PT_NULL,说明program header未使用
	cmp byte [ebx + 0], PT_NULL
	je .pt_null
	;传递参数(从右往左依次压入),调用函数mem_cpy(目的地址,源地址,字节数)
	push dword [ebx + 16] ;p_filesz
	mov eax, [ebx + 4] ;p_offset
	add eax, KERNEL_BIN_ADDR
	push eax
	push dword [ebx + 8] ;p_vaddr
	call mem_cpy
	add esp, 12 ;清理栈中的三个参数
.pt_null:
	add ebx, edx ;ebx指向下一个program header
	loop .each_segment
	ret

;逐字节拷贝,参数从右往左压栈
;mem_cpy(dst,src,size)
mem_cpy:
	cld	
	;备份ebp,ecx
	push ebp
	mov ebp, esp
	push ecx
	
	;逐字节拷贝
	mov edi, [ebp + 8] ;dst
	mov esi, [ebp + 12] ;src
	mov ecx, [ebp + 16] ;size
	rep movsb
	
	;恢复ecx,ebp
	pop ecx
	pop ebp
	ret
	
;32位模式下读取硬盘数据到内存中
;和read_disk_m16的区别就是把bx改成了ebx
;eax=LBA扇区号
;ebx=将数据写入的内存地址
;cx=读入的扇区数
read_disk_m32:
	mov esi, eax ;备份eax
	mov di, cx ;备份cx
	
	;第1步:设置要读取得扇区数
	mov al, cl
	out 0x1f2, al
	
	mov eax, esi ;恢复eax
	
	;第2步:将LBA地址存入0x1f3~0x1f6
	
	;LBA地址7~0位写入0x1f3
	out 0x1f3, al
	
	;LBA地址15~8位写入0x1f4
	mov cl, 8
	shr eax, cl
	out 0x1f4, al
	
	;LBA地址23~16位写入0x1f5
	shr eax, cl
	out 0x1f5, al
	
	;LBA地址27~24位写入0x1f6,并设置LBA模式
	shr eax, cl
	and al, 0x0f
	or al, 0xe0
	out 0x1f6, al
	
	;第3步:向0x1f7写入读命令(0x20)
	mov dx, 0x1f7
	mov al, 0x20
	out dx, al
;第4步:检测硬盘状态
.not_ready:
	nop
	in al, dx
	and al, 0x88 ;第4位为1表示硬盘控制器已经准备好数据传输
	cmp al, 0x08
	jnz .not_ready ;未准备好,继续检测
	
	;第5步:从0x1f0读数据
	;一个扇区512字节,每次读一个字,需要di*512/2=di*256次
	mov ax, di
	mov dx, 256
	mul dx
	mov cx, ax
	mov dx, 0x1f0
.on_read:
	in ax, dx
	mov [ebx], ax
	add ebx, 2
	loop .on_read
	ret
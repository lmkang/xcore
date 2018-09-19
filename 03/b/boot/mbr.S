	%include "loader.inc"
	
	org 0x7c00
	
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax
	mov sp, 0x7c00
	
	;从硬盘中读取loader到内存中,然后跳过去执行
	mov eax, LOADER_START_SECTOR
	mov bx, LOADER_BASE_ADDR
	mov cx, LOADER_SECTOR_COUNT
	call read_disk_m16
	jmp LOADER_BASE_ADDR + 0x300

;16位模式下读取硬盘数据到内存中
;eax=LBA扇区号
;bx=将数据写入的内存地址
;cx=读入的扇区数
read_disk_m16:
	mov esi, eax ;备份eax
	mov di, cx ;备份cx
	
	;第1步:设置要读取得扇区数
	mov dx, 0x1f2
	mov al, cl
	out dx, al
	
	mov eax, esi ;恢复eax
	
	;第2步:将LBA地址存入0x1f3~0x1f6
	
	;LBA地址7~0位写入0x1f3
	mov dx, 0x1f3
	out dx, al
	
	;LBA地址15~8位写入0x1f4
	mov dx, 0x1f4
	mov cl, 8
	shr eax, cl
	out dx, al
	
	;LBA地址23~16位写入0x1f5
	mov dx, 0x1f5
	shr eax, cl
	out dx, al
	
	;LBA地址27~24位写入0x1f6,并设置LBA模式
	mov dx, 0x1f6
	shr eax, cl
	and al, 0x0f
	or al, 0xe0
	out dx, al
	
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
	mov [bx], ax
	add bx, 2
	loop .on_read
	ret

	times 510-($-$$) db 0
	db 0x55, 0xaa
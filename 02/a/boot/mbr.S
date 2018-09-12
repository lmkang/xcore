	%include "boot.inc"
	
	org 0x7c00
	
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax
	mov sp, 0x7c00
	
	mov eax, LOADER_START_SECTOR
	mov bx, LOADER_BASE_ADDR
	mov cx, LOADER_SECTOR_COUNT
	call read_disk_m16
	jmp LOADER_BASE_ADDR
	
read_disk_m16:
	mov esi, eax
	mov di, cx
	
	mov al, cl
	out 0x1f2, al
	
	mov eax, esi
	
	out 0x1f3, al
	
	mov cl, 8
	shr eax, cl
	out 0x1f4, al
	
	shr eax, cl
	out 0x1f5, al
	
	shr eax, cl
	and al, 0x0f
	or al, 0xe0
	out 0x1f6, al
	
	mov dx, 0x1f7
	mov al, 0x20
	out dx, al
.not_ready:
	nop
	in al, dx
	and al, 0x88
	cmp al, 0x08
	jnz .not_ready
	
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
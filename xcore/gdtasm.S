[bits 32]
global flush_gdt

flush_gdt:
	mov eax, [esp + 4]
	lgdt [eax]
	
	;数据段
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov ss, ax
	;视频段
	mov ax, 0x18
	mov gs, ax
	
	jmp 0x08:.flush
.flush:
	ret
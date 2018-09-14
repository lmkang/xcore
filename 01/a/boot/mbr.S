	org 0x7c00
	
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov gs, ax
	mov sp, 0x7c00
	
	mov si, MESSAGE
msgloop:
	mov al, [si]
	cmp al, 0
	je final
	mov ah, 0x0e
	mov bx, 15
	int 0x10
	add si, 1
	jmp msgloop
final:
	hlt
	jmp final

MESSAGE:
	db "xcore/01/a hello,world!"
	db 0x0d, 0x0a
	db 0

	times 510-($-$$) db 0
	db 0x55, 0xaa

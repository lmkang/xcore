	org 0x7c00
	
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov sp, 0x7c00
	
	mov ax, 0xb800
	mov gs, ax
	mov bx, 0x00
	mov si, msg
msgloop:
	mov al, [si]
	cmp al, 0
	je final
	mov [gs:bx], al
	add bx, 1
	mov byte [gs:bx], 0xa4
	add bx, 1
	add si, 1
	jmp msgloop
final:
	hlt
	jmp final
msg:
	db "hello,world!"
	db 0

	times 510-($-$$) db 0
	db 0x55, 0xaa
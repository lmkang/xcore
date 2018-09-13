	%include "loader.inc"
	
	org LOADER_BASE_ADDR
	
	mov si, msg
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

msg:
	db "xcore/02/a hello,loader!"
	db 0x0d, 0x0a
	db 0

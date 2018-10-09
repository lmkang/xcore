[bits 32]
extern c_put_char
global put_char

put_char:
	xor eax, eax
	mov al, [gs:0] ;为了特权级检查
	mov al, [esp + 4]
	push eax
	call c_put_char
	add esp, 4
	ret
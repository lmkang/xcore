	%include "loader.inc"
	%include "boot.inc"
	
	org LOADER_BASE_ADDR
	
	LOADER_STACK_TOP equ LOADER_BASE_ADDR
	
	jmp loader_start
	
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
	
GDT_PTR:
	dw GDT_LIMIT
	dd GDT_BASE

MESSAGE:
	db "xcore/02/b hello,protect mode!"
	db 0
	
loader_start:
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
	
	mov bx, 0x00
	mov si, MESSAGE
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

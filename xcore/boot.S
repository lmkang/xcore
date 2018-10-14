	MBOOT_PAGE_ALIGN equ 1 << 0
	MBOOT_MEM_INFO equ 1 << 1
	MBOOT_HEADER_MAGIC equ 0x1BADB002
	MBOOT_HEADER_FLAGS equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
	MBOOT_CHECKSUM equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)
	
[bits 32]
section .init.text
	dd MBOOT_HEADER_MAGIC
	dd MBOOT_HEADER_FLAGS
	dd MBOOT_CHECKSUM

global start
global glb_mboot_ptr
extern entry

start:
	cli
	mov [glb_mboot_ptr], ebx
	mov esp, STACK_TOP
	and esp, 0xfffffff0
	mov ebp, 0
	call entry
	jmp $
	
section .init.data
stack:
	times 1024 db 0
	STACK_TOP equ $ - stack - 1
glb_mboot_ptr:
	dd 0
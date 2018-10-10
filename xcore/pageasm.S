[bits 32]
global flush_page
extern gdt_ptr

flush_page:
	sgdt [gdt_ptr] ;临时存储gdt_ptr
	mov ebx, [gdt_ptr + 2]
	or dword [ebx + 0x18 + 4], 0xc0000000
	add dword [gdt_ptr + 2], 0xc0000000
	add esp, 0xc0000000
	
	mov eax, 0x400000 ;PDT_BASE_ADDR
	mov cr3, eax
	mov eax, cr0
	or eax, 0x80000000
	mov cr0, eax
	lgdt [gdt_ptr] ;重新加载gdt_ptr
	ret
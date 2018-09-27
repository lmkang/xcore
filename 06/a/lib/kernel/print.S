	TI_GDT equ 0
	RPL0 equ 0
	SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0
	
[bits 32]
section .text
global put_char
;put_char
;将栈中的1个字符写入光标所在处
put_char:
	pushad
	;确保gs中保存的是SELECTOR_VIDEO
	mov ax, SELECTOR_VIDEO
	mov gs, ax
	;1、获取当前光标的位置
	;先获取高8位
	mov dx, 0x03d4
	mov al, 0x0e
	out dx, al
	mov dx, 0x03d5
	in al, dx
	mov ah, al
	;再获取低8位
	mov dx, 0x03d4
	mov al, 0x0f
	out dx, al
	mov dx, 0x03d5
	in al, dx
	;将光标存入bx
	mov bx, ax
	;2、获取栈中的字符
	;pushad压入4 * 8 = 32字节,加上主调函数的返回地址(4字节)等于36字节
	mov ecx, [esp + 36]
	;3、判断字符种类(回车符0xd、换行符0xa、退格符0x8)
	cmp cl, 0xd
	jz .is_carriage_return ;回车符
	cmp cl, 0xa
	jz .is_line_feed ;换行符
	cmp cl, 0x8
	jz .is_backspace ;退格符
	jmp .is_other ;其他字符
;退格:光标向前移一个显存位置,用空格/空字符0替换掉原来的字符
.is_backspace:
	dec bx
	shl bx, 1
	mov byte [gs:bx], 0x20 ;空格
	inc bx
	mov byte [gs:bx], 0x07 ;黑屏白字
	shr bx, 1
	jmp .set_cursor
;其他字符正常显示
.is_other:
	shl bx, 1
	mov [gs:bx], cl
	inc bx
	mov byte [gs:bx], 0x07
	shr bx, 1
	inc bx
	cmp bx, 2000
	jl .set_cursor
;回车/换行:都处理为光标移到下一行的行首
.is_line_feed:
.is_carriage_return:
	xor dx, dx ;被除数的高16位,清0
	mov ax, bx ;被除数的低16位
	mov si, 80
	div si
	;光标值减去余数便是取整
	sub bx, dx
	
	add bx, 80
	cmp bx, 2000
	jl .set_cursor
;滚屏:屏幕行范围是0~24,原理是将1~24行搬到0~23行
;再将第24行用空格填充
.roll_screen:
	cld
	;2000 - 80 = 1920个字符要搬运,共1920 * 2 = 3840字节
	;一次搬运4字节,共3840 / 4 = 960次
	mov ecx, 960
	mov esi, 0xc00b80a0 ;第1行行首
	mov edi, 0xc00b8000 ;第0行行首
	rep movsd
	
	;将最后一行填充为空格
	;最后一行首字符的第一个字节偏移:1920 * 2 = 3840
	mov ebx, 3840
	;一行是80个字符(160字节),每次清空一个字符(2字节)
	mov ecx, 80
.clear:
	mov word [gs:ebx], 0x0720 ;黑底白字的空格
	add ebx, 2
	loop .clear
	mov bx, 1920 ;光标置为最后一行的首字符
;将光标设置为bx的值
.set_cursor:
	;设置高8位
	mov dx, 0x03d4
	mov al, 0x0e
	out dx, al
	mov dx, 0x03d5
	mov al, bh
	out dx, al
	;设置低8位
	mov dx, 0x03d4
	mov al, 0x0f
	out dx, al
	mov dx, 0x03d5
	mov al, bl
	out dx, al
	;恢复环境,返回
	popad
	ret
	
global put_str
;打印以0字符结尾的字符串
;参数:栈中的字符串
put_str:
	;备份ebx,ecx
	push ebx
	push ecx
	;用ecx存储参数,清空
	xor ecx, ecx
	;获取字符串的地址
	mov ebx, [esp + 12];
.each_str:
	mov cl, [ebx]
	cmp cl, 0
	jz .str_end
	push ecx
	call put_char
	add esp, 4 ;回收参数所占的栈空间
	inc ebx
	jmp .each_str
.str_end:
	;恢复环境,返回
	pop ecx
	pop ebx
	ret
	
global put_int
;以16进制形式打印32位无符号整型,不带"0x"前缀,以小写显示
;参数:32位整型
put_int:
	pushad
	mov eax, [esp + 4 * 9] ;获取栈中参数
	mov edx, eax ;备份eax
	and eax, 0xffffffff
	cmp eax, 0
	jz .is_zero
	mov cl, 28
	;esi标识整型前缀是否为0
	;0代表前缀是0,1代表前缀不是0
	mov esi, 0
	mov eax, edx
	shr eax, 28
	and eax, 0xf
	or esi, eax
;遍历栈中的整型数值
.each_int:
	cmp cl, 0
	jl .int_end
	mov eax, edx
	shr eax, cl
	sub cl, 4
	and eax, 0xf
	mov edi, eax ;备份eax
	or eax, esi
	cmp eax, 0
	jz .each_int ;前缀为0,当前数也为0,不输出
	mov eax, edi ;恢复eax
	mov esi, 1 ;前缀不是0,把esi置为1
	cmp eax, 10
	jl .is_digit
	;字母a-f
	sub eax, 10
	add eax, 0x61 ;0x61是'a'的ASCII
	push eax
	call put_char
	add esp, 4
	jmp .each_int
;数字0-9
.is_digit:
	add eax, 0x30 ;0x30是'0'的ASCII
	push eax
	call put_char
	add esp, 4
	jmp .each_int
;栈中的整型数值为0
.is_zero:
	add eax, 0x30 ;0x30是'0'的ASCII
	push eax
	call put_char
	add esp, 4
.int_end:
	;恢复环境,返回
	popad
	ret

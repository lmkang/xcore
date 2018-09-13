### 02、保护模式  
a、从硬盘中读取loader到内存  
（1）先选择通道，往该通道的sector count寄存器中写入待操作的扇区数；  
（2）往该通道上的三个LBA寄存器写入扇区起始地址的低24位；  
（3）往device寄存器中写入LBA地址的27~24位，并设第6位为1（LBA模式），设第4位为0（master硬盘）；  
（4）往该通道上的command寄存器写入读取命令（0x20）；  
（5）读取该通道上的status寄存器，判断硬盘工作是否完成；  
（6）将硬盘数据读出。  
b、进入保护模式  
（1）打开A20Gate
```
in al, 0x92
or al, 0x02
out 0x92, al
```
（2）加载GDT
```
lgdt [GDT_PTR]
```
（3）清空流水线
```
jmp dword SELECTOR_CODE:p_mode_start
```
c、获取物理内存容量  


d、分页模式  


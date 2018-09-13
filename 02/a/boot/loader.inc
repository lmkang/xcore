	;loader的磁盘位置,在第2个扇区,在mbr之后
	LOADER_START_SECTOR equ 0x1
	
	;loader的大小,占4个扇区
	LOADER_SECTOR_COUNT equ 0x4
	
	;loader的起始物理地址
	LOADER_BASE_ADDR equ 0x900

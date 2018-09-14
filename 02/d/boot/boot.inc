;type,base,limit
%macro SEG_ASM 3
    dw (((%3) >> 12) & 0xffff), ((%2) & 0xffff)
    db (((%2) >> 16) & 0xff), (0x90 | (%1)), (0xC0 | (((%3) >> 28) & 0xf)), (((%2) >> 24) & 0xff)
%endmacro
	
	;Request Privilege Level
	RPL0 equ 0x0000
	RPL1 equ 0x0001
	RPL2 equ 0x0002
	RPL3 equ 0x0003
	
	;Table Indicator
	TI_GDT equ 0x0000
	TI_LDT equ 0x0004
	
	;page directory table address
	PAGE_DIR_TABLE_ADDR equ 0x100000
	
	;page:Present,R/W,U/S
	PG_P equ 1b
	PG_RW_R equ 00b
	PG_RW_W equ 10b
	PG_US_S equ 000b
	PG_US_U equ 100b
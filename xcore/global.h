#ifndef __GLOBAL_H
#define __GLOBAL_H

// selector
#define SELECTOR_KERNEL_CODE (0x1 << 3)
#define SELECTOR_KERNEL_DATA (0x2 << 3)
#define SELECTOR_VIDEO_DATA (0x3 << 3)
#define SELECTOR_USER_CODE (0x4 << 3 + 3)
#define SELECTOR_USER_DATA (0x5 << 3 + 3)

// idt entry attribute
#define IDT_ENTRY_P 1
#define IDT_ENTRY_DPL0 0
#define IDT_ENTRY_DPL3 3
#define IDT_ENTRY_32_TYPE 0xe // 32位门
#define IDT_ENTRY_16_TYPE 0x6 // 16位门,目前用不到
#define IDT_ENTRY_ATTR_DPL0 ((IDT_ENTRY_P << 7) + (IDT_ENTRY_DPL0 << 5) + IDT_ENTRY_32_TYPE)
#define IDT_ENTRY_ATTR_DPL3 ((IDT_ENTRY_P << 7) + (IDT_ENTRY_DPL3 << 5) + IDT_ENTRY_32_TYPE)

#endif
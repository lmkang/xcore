#ifndef __GLOBAL_H
#define __GLOBAL_H

// selector
#define SELECTOR_KERNEL_CODE (0x1 << 3)
#define SELECTOR_KERNEL_DATA (0x2 << 3)
#define SELECTOR_VIDEO_DATA (0x3 << 3)
#define SELECTOR_USER_CODE (0x4 << 3 + 3)
#define SELECTOR_USER_DATA (0x5 << 3 + 3)

#define PAGE_SIZE 4096

#endif
#ifndef __TYPES_H
#define __TYPES_H

typedef signed char int8_t;
typedef signed short int int16_t;
typedef signed int int32_t;
typedef signed long long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;

#define NULL ((void*) 0)
#define bool int
#define true 1
#define false 0

#define pgd_t uint32_t // 页目录数据类型
#define pte_t uint32_t // 页表数据类型

#endif
#ifndef __MALLOC_H
#define __MALLOC_H

#include "stm32f1xx_hal.h"
// 定义内存区域选择常量
#define SRAMIN 0
#define SRAMBANK 1

// 定义内存类型
#define MT_TYPE uint16_t

// MEM1区块大小定义
#define MEM1_BLOCK_SIZE 32 // MEM1区块大小为32字节
// MEM1最大尺寸定义
#define MEM1_MAX_SIZE 10 * 1024 // MEM1最大尺寸为40KB

// MEM1分配表大小定义
#define MEM1_ALLOC_TABLE_SIZE MEM1_MAX_SIZE / MEM1_BLOCK_SIZE // 根据MEM1的最大尺寸和区块大小计算分配表大小

// 定义空指针常量
#ifndef NULL
#define NULL 0
#endif

// 内存管理设备结构体定义
struct _m_mallco_dev {
    // 初始化内存管理
    void (*init)(uint8_t);
    // 获取指定内存区域的使用情况
    uint16_t (*perused)(uint8_t);
    // 内存区域基地址数组
    uint8_t *membase[SRAMBANK];
    // 内存映射表数组
    MT_TYPE *memmap[SRAMBANK];
    // 内存区域准备情况标志数组
    uint8_t memrdy[SRAMBANK];
};

// 外部声明：内存管理设备实例
extern struct _m_mallco_dev mallco_dev;

// 初始化指定的内存模块
void my_mem_init(uint8_t memx);

// 获取指定内存模块的已使用百分比
uint16_t my_mem_perused(uint8_t memx);

// 内存设置函数，用于设置内存区域的特定区域为指定值
void my_mem_set(void *s, uint8_t c, uint32_t count);

// 内存复制函数，用于将源内存区域复制到目标内存区域
void my_mem_copy(void *des, void *src, uint32_t n);

// 释放指定内存模块中的内存块
void myfree(uint8_t memx, void *ptr);

// 从指定内存模块分配指定大小的内存块
void *mymalloc(uint8_t memx, uint32_t size);

// 重新分配指定内存模块中指定内存块的大小
void *myrealloc(uint8_t memx, void *ptr, uint32_t size);

#endif

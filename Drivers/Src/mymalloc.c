/*
 * 文件名: malloc.c
 * 功能: 提供内存分配和释放的函数实现。
 */

#include "mymalloc.h"

// 定义内存块的起始地址
static __ALIGNED(64) uint8_t mem1base[MEM1_MAX_SIZE];

// 内存映射表，用于记录内存块的使用情况
static MT_TYPE mem1mapbase[MEM1_ALLOC_TABLE_SIZE];

// 内存表大小、内存块大小和内存大小的定义
const uint32_t memtblsize[SRAMBANK] = {MEM1_ALLOC_TABLE_SIZE};
const uint32_t memblksize[SRAMBANK] = {MEM1_BLOCK_SIZE};
const uint32_t memsize[SRAMBANK] = {
    MEM1_MAX_SIZE,
};

// 内存分配设备的结构体定义
struct _m_mallco_dev mallco_dev = {
    my_mem_init, my_mem_perused, mem1base, mem1mapbase, 0, /* 内存尚未被分配 */
};

/**
 * @brief       复制内存
 * @param       *des : 目标内存地址
 * @param       *src : 源内存地址
 * @param       n    : 需要复制的字节长度
 * @retval      无
 */
void my_mem_copy(void *des, void *src, uint32_t n)
{
    uint8_t *xdes = des;
    uint8_t *xsrc = src;

    // 循环复制n个字节
    while (n--)
        *xdes++ = *xsrc++;
}

/**
 * @brief       填充内存
 * @param       *s    : 目标内存地址
 * @param       c     : 需要填充的字节
 * @param       count : 需要填充的字节长度
 * @retval      无
 */
void my_mem_set(void *s, uint8_t c, uint32_t count)
{
    uint8_t *xs = s;

    // 循环填充count个字节
    while (count--)
        *xs++ = c;
}

/**
 * @brief       初始化内存
 * @param       memx : 内存标识
 * @retval      无
 */
void my_mem_init(uint8_t memx)
{
    uint8_t mttsize = sizeof(MT_TYPE); /* 获取内存映射表项的大小 */

    // 将内存映射表初始化为全0
    my_mem_set(mallco_dev.memmap[memx], 0, memtblsize[memx] * mttsize);

    // 标记内存初始化完成
    mallco_dev.memrdy[memx] = 1;
}

/**
 * @brief       获取内存使用率
 * @param       memx : 内存标识
 * @retval      使用率（0~1000）
 */
uint16_t my_mem_perused(uint8_t memx)
{
    uint32_t used = 0; // 已使用的内存块数
    uint32_t i;

    // 遍历内存映射表，统计已使用的内存块数
    for (i = 0; i < memtblsize[memx]; i++) {
        if (mallco_dev.memmap[memx][i])
            used++;
    }

    // 计算并返回内存使用率（百分比，最大1000）
    return (used * 1000) / (memtblsize[memx]);
}

/**
 * @brief       申请内存（内部函数）
 * @param       memx : 内存标识
 * @param       size : 申请内存块的大小
 * @retval      内存块的起始地址
 *   @arg       0 ~ 0XFFFFFFFE : 分配成功的内存块起始地址
 *   @arg       0XFFFFFFFF     : 分配失败
 */
static uint32_t my_mem_malloc(uint8_t memx, uint32_t size)
{
    signed long offset = 0;
    uint32_t nmemb;     /* 申请的内存块数 */
    uint32_t cmemb = 0; /* 连续的空闲内存块数 */
    uint32_t i;

    // 如果内存未初始化，则进行初始化
    if (!mallco_dev.memrdy[memx]) {
        mallco_dev.init(memx);
    }

    // 申请大小为0，直接返回失败
    if (size == 0)
        return 0XFFFFFFFF;

    nmemb = size / memblksize[memx]; /* 计算需要的内存块数 */

    // 如果申请的内存块大小不是内存块大小的整数倍，需要多申请一个内存块
    if (size % memblksize[memx])
        nmemb++;

    // 从内存表的末尾开始查找，寻找第一个能够连续分配的内存块
    for (offset = memtblsize[memx] - 1; offset >= 0; offset--) {
        if (!mallco_dev.memmap[memx][offset]) {
            cmemb++; /* 统计连续的空闲内存块数 */
        } else {
            cmemb = 0; /* 发现已使用的内存块，重新计数 */
        }

        // 如果找到足够数量的连续空闲内存块
        if (cmemb == nmemb) /* 找到了足够大的空间 */
        {
            // 标记这些内存块为已使用
            for (i = 0; i < nmemb; i++) {
                mallco_dev.memmap[memx][offset + i] = nmemb;
            }

            // 返回内存块的起始地址
            return (offset * memblksize[memx]);
        }
    }

    // 如果没有找到足够的连续空闲内存块，则返回失败
    return 0XFFFFFFFF;
}

/**
 * @brief       释放内存（内部函数）
 * @param       memx   : 内存标识
 * @param       offset : 内存块的起始地址偏移量
 * @retval      释放结果
 *   @arg       0, 释放成功;
 *   @arg       1, 释放失败;
 *   @arg       2, 无效的内存块;
 */
static uint8_t my_mem_free(uint8_t memx, uint32_t offset)
{
    int i;

    // 如果内存未初始化，则进行初始化
    if (!mallco_dev.memrdy[memx]) {
        mallco_dev.init(memx);
        return 1; /* 初始化失败 */
    }

    // 检查释放的内存块是否在内存池范围内
    if (offset < memsize[memx]) {
        int index = offset / memblksize[memx];      /* 计算内存块索引 */
        int nmemb = mallco_dev.memmap[memx][index]; /* 获取内存块大小 */

        // 将该内存块标记为未使用
        for (i = 0; i < nmemb; i++) {
            mallco_dev.memmap[memx][index + i] = 0;
        }

        return 0; /* 释放成功 */
    } else {
        return 2; /* 无效的内存块 */
    }
}

/**
 * @brief       释放内存（对外接口）
 * @param       memx : 内存标识
 * @param       ptr  : 需要释放的内存块的起始地址
 * @retval      无
 */
void myfree(uint8_t memx, void *ptr)
{
    uint32_t offset;

    // 如果指针为空，直接返回
    if (ptr == NULL)
        return;

    // 计算内存块的起始地址偏移量
    offset = (uint32_t)ptr - (uint32_t)mallco_dev.membase[memx];
    // 调用内部函数释放内存
    my_mem_free(memx, offset);
}

/**
 * @brief       申请内存（对外接口）
 * @param       memx : 内存标识
 * @param       size : 需要申请的内存大小
 * @retval      申请成功返回内存块的起始地址，失败返回NULL
 */
void *mymalloc(uint8_t memx, uint32_t size)
{
    uint32_t offset;
    // 调用内部函数申请内存
    offset = my_mem_malloc(memx, size);

    // 如果申请失败，返回NULL
    if (offset == 0XFFFFFFFF) {
        return NULL;
    } else /* 申请成功，返回内存块的起始地址 */
    {
        return (void *)((uint32_t)mallco_dev.membase[memx] + offset);
    }
}

/**
 * @brief       重新分配内存（对外接口）
 * @param       memx : 内存标识
 * @param       *ptr : 需要重新分配的内存块的起始地址
 * @param       size : 新的内存大小
 * @retval      重新分配成功返回新的内存块的起始地址，失败返回NULL
 */
void *myrealloc(uint8_t memx, void *ptr, uint32_t size)
{
    uint32_t offset;
    // 先调用内部函数申请新的内存块
    offset = my_mem_malloc(memx, size);

    // 如果申请失败，直接返回NULL
    if (offset == 0XFFFFFFFF) {
        return NULL;
    } else /* 申请成功，将原内存块的数据复制到新内存块并释放原内存块 */
    {
        // 如果ptr不为空，则复制数据
        if (ptr != NULL) {
            // 计算原内存块的起始地址偏移量
            // uint32_t old_offset = (uint32_t)ptr - (uint32_t)mallco_dev.membase[memx];
            // 复制数据到新内存块
            my_mem_copy((void *)((uint32_t)mallco_dev.membase[memx] + offset), ptr, size);
            // 释放原内存块
            myfree(memx, ptr);
        }
        // 返回新的内存块的起始地址
        return (void *)((uint32_t)mallco_dev.membase[memx] + offset);
    }
}

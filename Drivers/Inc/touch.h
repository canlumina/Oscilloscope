#ifndef __TOUCH_H__
#define __TOUCH_H__
#include "stm32f1xx.h"
//////////////////////////////////////////////////////////////////////////////////

// 触摸屏驱动（支持ADS7843/7846/UH7843/7846/XPT2046/TSC2046/OTT2001A等） 代码

//********************************************************************************
// V2.0修改说明
// 增加对电容触摸屏的支持(需要添加:ctiic.c和ott2001a.c两个文件)
//////////////////////////////////////////////////////////////////////////////////


#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 
//IO口地址映射
#define GPIOA_ODR_Addr    (GPIOA_BASE+12) //0x4001080C 
#define GPIOB_ODR_Addr    (GPIOB_BASE+12) //0x40010C0C 
#define GPIOC_ODR_Addr    (GPIOC_BASE+12) //0x4001100C 
#define GPIOD_ODR_Addr    (GPIOD_BASE+12) //0x4001140C 
#define GPIOE_ODR_Addr    (GPIOE_BASE+12) //0x4001180C 
#define GPIOF_ODR_Addr    (GPIOF_BASE+12) //0x40011A0C    
#define GPIOG_ODR_Addr    (GPIOG_BASE+12) //0x40011E0C    

#define GPIOA_IDR_Addr    (GPIOA_BASE+8) //0x40010808 
#define GPIOB_IDR_Addr    (GPIOB_BASE+8) //0x40010C08 
#define GPIOC_IDR_Addr    (GPIOC_BASE+8) //0x40011008 
#define GPIOD_IDR_Addr    (GPIOD_BASE+8) //0x40011408 
#define GPIOE_IDR_Addr    (GPIOE_BASE+8) //0x40011808 
#define GPIOF_IDR_Addr    (GPIOF_BASE+8) //0x40011A08 
#define GPIOG_IDR_Addr    (GPIOG_BASE+8) //0x40011E08 
 
//IO口操作,只对单一的IO口!
//确保n的值小于16!
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)  //输出 
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  //输入 

#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  //输出 
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  //输入 

#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)  //输出 
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)  //输入 

#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)  //输出 
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)  //输入 

#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)  //输出 
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)  //输入

#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)  //输出 
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)  //输入

#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)  //输出 
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)  //输入

#define TP_PRES_DOWN 0x80 // 触屏被按下
#define TP_CATH_PRES 0x40 // 有按键按下了

#define OTT_MAX_TOUCH 5

// 触摸屏控制器
typedef struct
{
    uint8_t (*init)(void);     // 初始化触摸屏控制器
    uint8_t (*scan)(uint8_t);  // 扫描触摸屏.0,屏幕扫描;1,物理坐标;
    void (*adjust)(void);      // 触摸屏校准
    uint16_t x[OTT_MAX_TOUCH]; // 当前坐标
    uint16_t y[OTT_MAX_TOUCH]; // 电容屏有最多5组坐标,电阻屏则用x[0],y[0]代表:此次扫描时,触屏的坐标,用
                               // x[4],y[4]存储第一次按下时的坐标.
    uint8_t sta;               // 笔的状态
                               // b7:按下1/松开0;
                               // b6:0,没有按键按下;1,有按键按下.
                               // b5:保留
                               // b4~b0:电容触摸屏按下的点数(0,表示未按下,1表示按下)
    /////////////////////触摸屏校准参数(电容屏不需要校准)//////////////////////
    float xfac;
    float yfac;
    short xoff;
    short yoff;
    // 新增的参数,当触摸屏的左右上下完全颠倒时需要用到.
    // b0:0,竖屏(适合左右为X坐标,上下为Y坐标的TP)
    //    1,横屏(适合左右为Y坐标,上下为X坐标的TP)
    // b1~6:保留.
    // b7:0,电阻屏
    //    1,电容屏
    uint8_t touchtype;
} _m_tp_dev;

extern _m_tp_dev tp_dev; // 触屏控制器在touch.c里面定义

// 电阻屏芯片连接引脚
#define PEN PFin(10)  // PF10 INT
#define DOUT PFin(8)  // PF8  MISO
#define TDIN PFout(9) // PF9  MOSI
#define TCLK PBout(1) // PB1  SCLK
#define TCS PBout(2)  // PB2  CS

// 电阻屏函数
void TP_Write_Byte(uint8_t num);                                                                                                             // 向控制芯片写入一个数据
uint16_t TP_Read_AD(uint8_t CMD);                                                                                                            // 读取AD转换值
uint16_t TP_Read_XOY(uint8_t xy);                                                                                                            // 带滤波的坐标读取(X/Y)
uint8_t TP_Read_XY(uint16_t *x, uint16_t *y);                                                                                                // 双方向读取(X+Y)
uint8_t TP_Read_XY2(uint16_t *x, uint16_t *y);                                                                                               // 带加强滤波的双方向坐标读取
void TP_Drow_Touch_Point(uint16_t x, uint16_t y, uint16_t color);                                                                            // 画一个坐标校准点
void TP_Draw_Big_Point(uint16_t x, uint16_t y, uint16_t color);                                                                              // 画一个大点
void tp_save_adjust_data(void);                                                                                                              // 保存校准参数
uint8_t TP_Get_Adjdata(void);                                                                                                                // 读取校准参数
void tp_adjust(void);                                                                                                                        // 触摸屏校准
void TP_Adj_Info_Show(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t fac); // 显示校准信息
// 电阻屏/电容屏 共用函数
uint8_t TP_Scan(uint8_t tp); // 扫描
uint8_t TP_Init(void);       // 初始化

#endif

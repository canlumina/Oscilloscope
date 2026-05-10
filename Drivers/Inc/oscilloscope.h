#ifndef __OSCILLOSCOPE_H
#define __OSCILLOSCOPE_H

#include "stm32f1xx.h"

/* ============================================================
 * 显示区域布局（横屏 320x240）
 *
 *  ┌────────────────────────┬──────┐  y=0
 *  │  状态栏 320x16          │       │
 *  ├────────────────────────┤ Vpp.. │  y=16
 *  │                          │ Max.. │
 *  │   波形区 240x224         │ Min.. │
 *  │   10列x8行（每格24x28）   │ Avg.. │
 *  │                          │ Frq.. │
 *  │                          │ Trg.. │
 *  │                          │ K0..  │
 *  │                          │ K1..  │
 *  │                          │ K2..  │
 *  └────────────────────────┴──────┘  y=239
 *  x=0                       240      319
 *
 * 网格：每格 24px(水平) x 28px(垂直)
 *       水平10格 = 240px
 *       垂直 8格 = 224px
 * ============================================================ */

#define OSC_WAVE_X      0
#define OSC_WAVE_Y      16
#define OSC_WAVE_W      240
#define OSC_WAVE_H      224
#define OSC_GRID_COLS   10
#define OSC_GRID_ROWS   8
#define OSC_CELL_W      24   /* 每格宽度像素 */
#define OSC_CELL_H      28   /* 每格高度像素 */

/* 电压量程档位 */
#define VSCALE_COUNT    6
extern const float    g_vscale_vpp[VSCALE_COUNT];   /* 满屏峰峰值 */
extern const char * const g_vscale_str[VSCALE_COUNT];

/* 触发模式 */
typedef enum {
    TRIG_AUTO   = 0,  /* 自动：无触发信号也刷新 */
    TRIG_NORMAL = 1,  /* 普通：有触发才刷新 */
    TRIG_SINGLE = 2,  /* 单次：触发一次停止 */
} TrigMode;

/* 触发边沿 */
typedef enum {
    TRIG_EDGE_RISE = 0,
    TRIG_EDGE_FALL = 1,
} TrigEdge;

/* 示波器状态 */
typedef struct {
    uint8_t   vscale_idx;   /* 电压量程索引 */
    uint8_t   timebase_idx; /* 时基索引 */
    float     trig_level;   /* 触发电平(V) */
    TrigMode  trig_mode;
    TrigEdge  trig_edge;
    uint8_t   run;          /* 1=运行 0=停止 */
    uint8_t   single_done;  /* 单次触发完成标志 */
    /* 测量结果 */
    float     meas_vpp;
    float     meas_vmax;
    float     meas_vmin;
    float     meas_vavg;
    float     meas_freq;
} OscState;

extern OscState g_osc;

/* 函数声明 */
void Osc_Init(void);
void Osc_DrawFrame(void);       /* 画静态框架（只需调用一次） */
void Osc_DrawGrid(void);        /* 画网格 */
void Osc_DrawWave(uint32_t *adc_buf, uint16_t len);  /* 画波形（增量擦+画，CH1+CH2） */
void Osc_DrawTrigLine(void);    /* 画触发电平线（增量擦+画） */
void Osc_UpdateStatusBar(void); /* 刷新状态栏（按键事件后调） */
void Osc_DrawInfoBarLabels(void);/* 信息栏静态标签（只需一次） */
void Osc_UpdateInfoBar(void);   /* 信息栏数值增量刷新 */
void Osc_Measure(uint32_t *adc_buf, uint16_t len);   /* 计算测量值（仅 CH1） */
uint16_t Osc_FindTrigger(uint32_t *adc_buf, uint16_t len); /* 找触发点（CH1） */
void Osc_InvalidateWave(void);  /* 让波形缓存失效（量程/时基变化时调） */

#endif /* __OSCILLOSCOPE_H */

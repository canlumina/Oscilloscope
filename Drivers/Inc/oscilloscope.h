#ifndef __OSCILLOSCOPE_H
#define __OSCILLOSCOPE_H

#include "stm32f1xx.h"

/* ============================================================
 * 显示区域布局（横屏 320x240）
 *
 *  ┌────────────────────────┬──────────┐  y=0
 *  │  状态栏 320x16          │           │
 *  ├────────────────────────┼──────────┤  y=16
 *  │                          │ Vpp  X X │
 *  │                          │ Max  X X │
 *  │                          │ Min  X X │
 *  │   波形区 240x192         │ Avg  X X │
 *  │   10列x8行（每格24x24）   │ Frq  X X │
 *  │                          │ Thi  X X │
 *  │                          │ Tlo  X X │
 *  │                          │ Trg  X   │
 *  ├────────────────────────┴──────────┤  y=208
 *  │ K0:Tbase  K1:Vscale  K2:Run/Stp    │  底部按键栏 32px
 *  └─────────────────────────────────────┘  y=239
 *
 * 网格：每格 24px(水平) x 24px(垂直)
 *       水平10格 = 240px，垂直 8格 = 192px
 * ============================================================ */

#define OSC_WAVE_X      0
#define OSC_WAVE_Y      16
#define OSC_WAVE_W      240
#define OSC_WAVE_H      192
#define OSC_GRID_COLS   10
#define OSC_GRID_ROWS   8
#define OSC_CELL_W      24   /* 每格宽度像素 */
#define OSC_CELL_H      24   /* 每格高度像素 */

/* 底部按键提示栏 */
#define OSC_KEYBAR_Y    (OSC_WAVE_Y + OSC_WAVE_H)        /* 208 */
#define OSC_KEYBAR_H    (240 - OSC_KEYBAR_Y)             /* 32  */

/* 电压量程档位 — 默认 3.3V 档专为 STM32 GPIO 全幅信号优化 */
#define VSCALE_COUNT    7
extern const float    g_vscale_vpp[VSCALE_COUNT];   /* 满屏峰峰值 */
extern const char * const g_vscale_str[VSCALE_COUNT];

/* 触发模式 */
typedef enum {
    TRIG_AUTO   = 0,
    TRIG_NORMAL = 1,
    TRIG_SINGLE = 2,
} TrigMode;

/* 触发边沿 */
typedef enum {
    TRIG_EDGE_RISE = 0,
    TRIG_EDGE_FALL = 1,
} TrigEdge;

/* 单通道测量结果 */
typedef struct {
    float vpp;
    float vmax;
    float vmin;
    float vavg;
    float freq;        /* Hz */
    float t_high_us;   /* 高电平脉宽（µs；< 0 = 无法测量） */
    float t_low_us;    /* 低电平脉宽（µs；< 0 = 无法测量） */
} OscChannelMeas;

/* 示波器状态 */
typedef struct {
    uint8_t   vscale_idx;
    uint8_t   timebase_idx;
    float     trig_level;
    TrigMode  trig_mode;
    TrigEdge  trig_edge;
    uint8_t   run;
    uint8_t   single_done;
    /* 双通道测量结果 */
    OscChannelMeas ch1;
    OscChannelMeas ch2;
} OscState;

extern OscState g_osc;

/* 函数声明 */
void Osc_Init(void);
void Osc_DrawFrame(void);
void Osc_DrawGrid(void);
void Osc_DrawWave(uint32_t *adc_buf, uint16_t len);
void Osc_DrawTrigLine(void);
void Osc_UpdateStatusBar(void);
void Osc_DrawInfoBarLabels(void);
void Osc_UpdateInfoBar(void);
void Osc_DrawKeyHints(void);          /* 底部按键提示栏（只画一次） */
void Osc_ClearWaveArea(void);         /* 清波形区+重画网格+重置缓存（切档时调） */
void Osc_Measure(uint32_t *adc_buf, uint16_t len);     /* 计算 CH1+CH2 测量值 */
uint16_t Osc_FindTrigger(uint32_t *adc_buf, uint16_t len);
void Osc_InvalidateWave(void);

#endif /* __OSCILLOSCOPE_H */

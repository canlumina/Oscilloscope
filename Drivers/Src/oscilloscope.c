#include "oscilloscope.h"
#include "lcd.h"
#include "adc_sample.h"
#include <string.h>

/* lcd.h 没有以下颜色，本文件局部补齐 */
#ifndef ORANGE
#define ORANGE      0xFD20    /* RGB565 橙色 */
#endif
#define LIGHT_GRAY  LGRAY     /* 浅灰 */
#define DARK_GRAY   GRAY      /* 深灰（lcd.h 中 GRAY=0x8430） */

/* lcd_show_string 字号常量（项目 lcd 模块以高度像素表示字号，常用 12/16/24） */
#define OSC_FONT_SIZE  12

/* ============================================================
 * 电压量程表（满屏8格显示的总电压范围）
 * ============================================================ */
const float g_vscale_vpp[VSCALE_COUNT] = {
    0.5f,   /* 62.5mV/div */
    1.0f,   /* 125mV/div  */
    2.0f,   /* 250mV/div  */
    4.0f,   /* 500mV/div  */
    8.0f,   /* 1V/div     */
    16.0f,  /* 2V/div（已超3.3V，仅搭配分压探头使用）*/
};

const char * const g_vscale_str[VSCALE_COUNT] = {
    "62.5mV", "125mV", "250mV", "500mV", "1.00V", "2.00V",
};

OscState g_osc = {
    .vscale_idx   = 3,              /* 默认500mV/div */
    .timebase_idx = 3,              /* 默认5ms/div */
    .trig_level   = 1.65f,          /* 默认触发电平1.65V(中间) */
    .trig_mode    = TRIG_AUTO,
    .trig_edge    = TRIG_EDGE_RISE,
    .run          = 1,
    .single_done  = 0,
};

/* ============================================================
 * lcd 模块只导出 lcd_show_string(x,y,w,h,size,p,color)，
 * 背景色取自全局 g_back_color；统一在这里包装。
 * ============================================================ */
static void osc_show_str(uint16_t x, uint16_t y, const char *s,
                         uint16_t fc, uint16_t bc, uint8_t size)
{
    uint32_t saved_bg = g_back_color;
    g_back_color = bc;
    /* 给一个足够大的 width / 1 行高度，避免折行 */
    lcd_show_string(x, y, 240, size, size, (char *)s, fc);
    g_back_color = saved_bg;
}

/* 简易浮点显示：int_w 整数位、dec_w 小数位（手动 ascii，避免 stdio 依赖） */
static void osc_show_float(uint16_t x, uint16_t y, float v,
                           uint8_t int_w, uint8_t dec_w,
                           uint16_t fc, uint16_t bc, uint8_t size)
{
    char buf[16];
    int  idx = 0, neg = 0;
    if (v < 0) { neg = 1; v = -v; }

    long whole = (long)v;
    float frac = v - (float)whole;

    long scale = 1;
    for (uint8_t k = 0; k < dec_w; k++) scale *= 10;
    long fracInt = (long)(frac * (float)scale + 0.5f);
    if (fracInt >= scale) { whole++; fracInt -= scale; }

    /* 整数部分（带可选负号） */
    char tmp[12];
    int  ti = 0;
    if (whole == 0) tmp[ti++] = '0';
    while (whole > 0) { tmp[ti++] = (char)('0' + whole % 10); whole /= 10; }
    if (neg) tmp[ti++] = '-';
    /* 左侧空格补齐到 int_w（含负号） */
    while (ti < int_w) tmp[ti++] = ' ';
    while (ti > 0) buf[idx++] = tmp[--ti];

    if (dec_w > 0) {
        buf[idx++] = '.';
        long div = scale / 10;
        for (uint8_t k = 0; k < dec_w; k++) {
            buf[idx++] = (char)('0' + (fracInt / (div ? div : 1)) % 10);
            if (div) div /= 10;
        }
    }
    buf[idx] = '\0';
    osc_show_str(x, y, buf, fc, bc, size);
}

/* ============================================================
 * 内部：ADC值 → 屏幕Y坐标
 * ============================================================ */
static uint16_t adc_to_y(uint16_t raw)
{
    float v      = ADC_Sample_ToVoltage(raw);
    float vscale = g_vscale_vpp[g_osc.vscale_idx];
    /* 以中心1.65V为基准，向上正偏移 */
    float offset = (v - 1.65f) / vscale;  /* -0.5 ~ +0.5 */
    if (offset >  0.5f) offset =  0.5f;
    if (offset < -0.5f) offset = -0.5f;
    int16_t y = (int16_t)(OSC_WAVE_Y + OSC_WAVE_H / 2 - (int16_t)(offset * OSC_WAVE_H));
    if (y < OSC_WAVE_Y) y = OSC_WAVE_Y;
    if (y >= OSC_WAVE_Y + OSC_WAVE_H) y = OSC_WAVE_Y + OSC_WAVE_H - 1;
    return (uint16_t)y;
}

/* ============================================================
 * 增量绘制缓存（双通道）
 *  s_prev_ys1/2  上一帧 CH1/CH2 每列 Y，用于擦除
 *  s_has_prev    第一次画时不需要擦
 *  s_prev_trig_y 上一次触发线 Y
 *  s_has_trig    触发线是否已画过
 * ============================================================ */
static uint16_t s_prev_ys1[OSC_WAVE_W];
static uint16_t s_prev_ys2[OSC_WAVE_W];
static uint8_t  s_has_prev    = 0;
static uint16_t s_prev_trig_y = 0;
static uint8_t  s_has_trig    = 0;

void Osc_InvalidateWave(void)
{
    s_has_prev = 0;
}

/* ============================================================
 * 初始化
 * ============================================================ */
void Osc_Init(void)
{
    lcd_clear(BLACK);
    Osc_DrawFrame();
    Osc_DrawGrid();
    Osc_UpdateStatusBar();
    Osc_DrawInfoBarLabels();   /* 静态标签 + 按键提示，只画一次 */
    Osc_UpdateInfoBar();        /* 数值首次显示 */
    Osc_DrawTrigLine();        /* 触发线首次绘制 */
}

/* ============================================================
 * 画静态边框（横屏 320x240，右侧 80px 为信息栏）
 * ============================================================ */
void Osc_DrawFrame(void)
{
    /* 波形区边框 */
    lcd_draw_rectangle(OSC_WAVE_X, OSC_WAVE_Y,
                       OSC_WAVE_X + OSC_WAVE_W - 1,
                       OSC_WAVE_Y + OSC_WAVE_H - 1, GRAY);
    /* 状态栏底分隔线（贯穿整屏） */
    lcd_draw_hline(0, OSC_WAVE_Y - 1, 320, GRAY);
    /* 右侧信息栏左边界（贯穿整屏） */
    lcd_draw_line(OSC_WAVE_W, 0, OSC_WAVE_W, 239, GRAY);
}

/* ============================================================
 * 画网格（虚线 + 中心十字）
 * ============================================================ */
void Osc_DrawGrid(void)
{
    uint16_t i, j;
    /* 垂直网格线 */
    for (i = 1; i < OSC_GRID_COLS; i++) {
        uint16_t x = OSC_WAVE_X + i * OSC_CELL_W;
        for (j = OSC_WAVE_Y; j < OSC_WAVE_Y + OSC_WAVE_H; j += 4) {
            lcd_draw_point(x, j, LGRAY);
        }
    }
    /* 水平网格线 */
    for (i = 1; i < OSC_GRID_ROWS; i++) {
        uint16_t y = OSC_WAVE_Y + i * OSC_CELL_H;
        for (j = OSC_WAVE_X; j < OSC_WAVE_X + OSC_WAVE_W; j += 4) {
            lcd_draw_point(j, y, LGRAY);
        }
    }
    /* 中心十字轴 */
    lcd_draw_hline(OSC_WAVE_X, OSC_WAVE_Y + OSC_WAVE_H / 2, OSC_WAVE_W, GRAY);
    lcd_draw_line(OSC_WAVE_X + OSC_WAVE_W / 2, OSC_WAVE_Y,
                  OSC_WAVE_X + OSC_WAVE_W / 2, OSC_WAVE_Y + OSC_WAVE_H - 1, GRAY);
}

/* ============================================================
 * 画双通道波形（增量擦+画）
 *  buf 为打包样本：低 16 位 = CH1，高 16 位 = CH2
 *  CH1 = GREEN，CH2 = CYAN
 * ============================================================ */
void Osc_DrawWave(uint32_t *adc_buf, uint16_t len)
{
    if (len < 2) return;

    uint16_t cnt = (len < OSC_WAVE_W) ? len : OSC_WAVE_W;

    /* 1) 擦上一帧两路 */
    if (s_has_prev) {
        for (uint16_t i = 1; i < cnt; i++) {
            lcd_draw_line((uint16_t)(OSC_WAVE_X + i - 1), s_prev_ys1[i - 1],
                          (uint16_t)(OSC_WAVE_X + i),     s_prev_ys1[i],
                          BLACK);
            lcd_draw_line((uint16_t)(OSC_WAVE_X + i - 1), s_prev_ys2[i - 1],
                          (uint16_t)(OSC_WAVE_X + i),     s_prev_ys2[i],
                          BLACK);
        }
    }

    /* 2) 计算并画新帧两路 */
    s_prev_ys1[0] = adc_to_y(ADC_SAMPLE_CH1(adc_buf[0]));
    s_prev_ys2[0] = adc_to_y(ADC_SAMPLE_CH2(adc_buf[0]));
    for (uint16_t i = 1; i < cnt; i++) {
        s_prev_ys1[i] = adc_to_y(ADC_SAMPLE_CH1(adc_buf[i]));
        s_prev_ys2[i] = adc_to_y(ADC_SAMPLE_CH2(adc_buf[i]));
        lcd_draw_line((uint16_t)(OSC_WAVE_X + i - 1), s_prev_ys1[i - 1],
                      (uint16_t)(OSC_WAVE_X + i),     s_prev_ys1[i],
                      GREEN);
        lcd_draw_line((uint16_t)(OSC_WAVE_X + i - 1), s_prev_ys2[i - 1],
                      (uint16_t)(OSC_WAVE_X + i),     s_prev_ys2[i],
                      CYAN);
    }
    s_has_prev = 1;

    /* 3) 触发线被擦除路径啃掉的像素，重画恢复 */
    Osc_DrawTrigLine();
}

/* ============================================================
 * 画触发电平线（虚线，增量擦+画）
 *  - y 未变：只重画一次（恢复被波形擦除路径破坏的像素，无可见闪烁）
 *  - y 变化：先用黑色擦掉旧虚线，再画新位置
 * ============================================================ */
void Osc_DrawTrigLine(void)
{
    float vscale = g_vscale_vpp[g_osc.vscale_idx];
    float offset = (g_osc.trig_level - 1.65f) / vscale;
    if (offset >  0.5f) offset =  0.5f;
    if (offset < -0.5f) offset = -0.5f;
    uint16_t y = (uint16_t)(OSC_WAVE_Y + OSC_WAVE_H / 2 - (int16_t)(offset * OSC_WAVE_H));

    if (s_has_trig && s_prev_trig_y != y) {
        /* 擦旧位置 */
        for (uint16_t x = OSC_WAVE_X; x < OSC_WAVE_X + OSC_WAVE_W; x += 6) {
            lcd_draw_hline(x, s_prev_trig_y, 3, BLACK);
        }
        lcd_draw_point(OSC_WAVE_X,     s_prev_trig_y,     BLACK);
        lcd_draw_point(OSC_WAVE_X + 1, s_prev_trig_y - 1, BLACK);
        lcd_draw_point(OSC_WAVE_X + 1, s_prev_trig_y + 1, BLACK);
    }

    /* 画新位置（即使 y 未变也重画一次，恢复被波形擦除路径啃掉的像素） */
    for (uint16_t x = OSC_WAVE_X; x < OSC_WAVE_X + OSC_WAVE_W; x += 6) {
        lcd_draw_hline(x, y, 3, YELLOW);
    }
    lcd_draw_point(OSC_WAVE_X,     y,     YELLOW);
    lcd_draw_point(OSC_WAVE_X + 1, y - 1, YELLOW);
    lcd_draw_point(OSC_WAVE_X + 1, y + 1, YELLOW);

    s_prev_trig_y = y;
    s_has_trig    = 1;
}

/* ============================================================
 * 找触发点（CH1 触发）
 * ============================================================ */
uint16_t Osc_FindTrigger(uint32_t *adc_buf, uint16_t len)
{
    uint16_t trig_raw = (uint16_t)(g_osc.trig_level / 3.3f * 4095.0f);
    uint16_t i;

    if (len <= OSC_WAVE_W) return 0;

    if (g_osc.trig_edge == TRIG_EDGE_RISE) {
        for (i = 1; i < len - OSC_WAVE_W; i++) {
            if (ADC_SAMPLE_CH1(adc_buf[i - 1]) <  trig_raw &&
                ADC_SAMPLE_CH1(adc_buf[i])     >= trig_raw)
                return i;
        }
    } else {
        for (i = 1; i < len - OSC_WAVE_W; i++) {
            if (ADC_SAMPLE_CH1(adc_buf[i - 1]) >  trig_raw &&
                ADC_SAMPLE_CH1(adc_buf[i])     <= trig_raw)
                return i;
        }
    }
    return 0;
}

/* ============================================================
 * 测量计算（仅 CH1，CH2 仅画线）
 * ============================================================ */
void Osc_Measure(uint32_t *adc_buf, uint16_t len)
{
    if (len == 0) return;

    uint32_t sum  = 0;
    uint16_t vmax = 0, vmin = 4095;
    for (uint16_t i = 0; i < len; i++) {
        uint16_t v = ADC_SAMPLE_CH1(adc_buf[i]);
        sum += v;
        if (v > vmax) vmax = v;
        if (v < vmin) vmin = v;
    }

    g_osc.meas_vmax = ADC_Sample_ToVoltage(vmax);
    g_osc.meas_vmin = ADC_Sample_ToVoltage(vmin);
    g_osc.meas_vpp  = g_osc.meas_vmax - g_osc.meas_vmin;
    g_osc.meas_vavg = ADC_Sample_ToVoltage((uint16_t)(sum / len));

    /* 频率估算：以 vavg 为基准计上升过零点 */
    uint16_t mid_raw   = (uint16_t)(g_osc.meas_vavg / 3.3f * 4095.0f);
    uint16_t cross_cnt = 0;
    for (uint16_t i = 1; i < len; i++) {
        if (ADC_SAMPLE_CH1(adc_buf[i - 1]) <  mid_raw &&
            ADC_SAMPLE_CH1(adc_buf[i])     >= mid_raw)
            cross_cnt++;
    }

    /* 采样率(Hz) = 1MHz / (ARR+1) */
    float sample_rate = 1000000.0f / (float)(TIM3->ARR + 1);
    if (cross_cnt >= 2 && len > 1) {
        g_osc.meas_freq = sample_rate * (float)(cross_cnt - 1) / (float)(len - 1);
    } else {
        g_osc.meas_freq = 0.0f;
    }
}

/* ============================================================
 * 刷新顶部状态栏（横屏 320 宽）
 * ============================================================ */
void Osc_UpdateStatusBar(void)
{
    /* 清空状态栏（不动右栏分隔线 x=240 那条竖线被覆盖也没关系，DrawFrame 不再调用） */
    lcd_fill(0, 0, 319, 15, BLACK);

    /* 时基 */
    osc_show_str(0,   0, g_timebase_str[g_osc.timebase_idx], CYAN,   BLACK, OSC_FONT_SIZE);
    /* 量程 */
    osc_show_str(72,  0, g_vscale_str[g_osc.vscale_idx],     YELLOW, BLACK, OSC_FONT_SIZE);
    /* 触发模式 */
    {
        static const char * const tmode[] = {"AUTO", "NORM", "SING"};
        osc_show_str(132, 0, tmode[g_osc.trig_mode], ORANGE, BLACK, OSC_FONT_SIZE);
    }
    /* 触发边沿 */
    osc_show_str(168, 0, (g_osc.trig_edge == TRIG_EDGE_RISE) ? "/\\" : "\\/",
                 WHITE, BLACK, OSC_FONT_SIZE);
    /* 运行状态 */
    osc_show_str(198, 0, g_osc.run ? "RUN" : "STP",
                 g_osc.run ? GREEN : RED, BLACK, OSC_FONT_SIZE);
    /* 标题（占用 240..319 段）双通道颜色提示：CH1 绿 / CH2 青 */
    osc_show_str(244, 0, "CH1",    GREEN, BLACK, OSC_FONT_SIZE);
    osc_show_str(264, 0, "+",      LIGHT_GRAY, BLACK, OSC_FONT_SIZE);
    osc_show_str(272, 0, "CH2",    CYAN,  BLACK, OSC_FONT_SIZE);

    /* 状态栏被清后右栏左边界顶端那一截也丢了，补回来 */
    lcd_draw_line(OSC_WAVE_W, 0, OSC_WAVE_W, 15, GRAY);
}

/* ============================================================
 * 信息栏布局常量（横屏右栏 244..319 × 16..239）
 * ============================================================ */
#define INFO_LX     (OSC_WAVE_W + 4)   /* 244 标签 x */
#define INFO_VX     (INFO_LX + 24)     /* 268 数值 x */
#define INFO_UX     (INFO_LX + 48)     /* 292 单位 x */
#define INFO_ROW_H  24
#define INFO_VAL_X1 INFO_VX            /* 数值+单位 lcd_fill 起点 */
#define INFO_VAL_X2 318                /* 数值+单位 lcd_fill 终点 */

/* ============================================================
 * 信息栏静态部分：标签 + 按键提示，只在 Init 时画一次
 * ============================================================ */
void Osc_DrawInfoBarLabels(void)
{
    uint16_t y = OSC_WAVE_Y + 2;        /* 18 */

    osc_show_str(INFO_LX, y, "Vpp:", LIGHT_GRAY, BLACK, OSC_FONT_SIZE); y += INFO_ROW_H;
    osc_show_str(INFO_LX, y, "Max:", LIGHT_GRAY, BLACK, OSC_FONT_SIZE); y += INFO_ROW_H;
    osc_show_str(INFO_LX, y, "Min:", LIGHT_GRAY, BLACK, OSC_FONT_SIZE); y += INFO_ROW_H;
    osc_show_str(INFO_LX, y, "Avg:", LIGHT_GRAY, BLACK, OSC_FONT_SIZE); y += INFO_ROW_H;
    osc_show_str(INFO_LX, y, "Frq:", LIGHT_GRAY, BLACK, OSC_FONT_SIZE); y += INFO_ROW_H;
    osc_show_str(INFO_LX, y, "Trg:", LIGHT_GRAY, BLACK, OSC_FONT_SIZE); y += INFO_ROW_H;

    /* 按键提示（不会变） */
    osc_show_str(INFO_LX, y, "K0:Tbase",   DARK_GRAY, BLACK, OSC_FONT_SIZE); y += INFO_ROW_H;
    osc_show_str(INFO_LX, y, "K1:Vscale",  DARK_GRAY, BLACK, OSC_FONT_SIZE); y += INFO_ROW_H;
    osc_show_str(INFO_LX, y, "K2:Run/Stp", DARK_GRAY, BLACK, OSC_FONT_SIZE);
}

/* ============================================================
 * 信息栏数值刷新：只擦"数值+单位"那一小段，标签不动
 * 每行擦 INFO_VAL_X1..INFO_VAL_X2 × 12px，远小于整栏 fill
 * ============================================================ */
void Osc_UpdateInfoBar(void)
{
    uint16_t y = OSC_WAVE_Y + 2;          /* 18 */

    /* Vpp */
    lcd_fill(INFO_VAL_X1, y, INFO_VAL_X2, y + OSC_FONT_SIZE - 1, BLACK);
    osc_show_float(INFO_VX, y, g_osc.meas_vpp,  1, 2, GREEN,  BLACK, OSC_FONT_SIZE);
    osc_show_str  (INFO_UX, y, "V",   GREEN,  BLACK, OSC_FONT_SIZE);
    y += INFO_ROW_H;

    /* Max */
    lcd_fill(INFO_VAL_X1, y, INFO_VAL_X2, y + OSC_FONT_SIZE - 1, BLACK);
    osc_show_float(INFO_VX, y, g_osc.meas_vmax, 1, 2, YELLOW, BLACK, OSC_FONT_SIZE);
    osc_show_str  (INFO_UX, y, "V",   YELLOW, BLACK, OSC_FONT_SIZE);
    y += INFO_ROW_H;

    /* Min */
    lcd_fill(INFO_VAL_X1, y, INFO_VAL_X2, y + OSC_FONT_SIZE - 1, BLACK);
    osc_show_float(INFO_VX, y, g_osc.meas_vmin, 1, 2, CYAN,   BLACK, OSC_FONT_SIZE);
    osc_show_str  (INFO_UX, y, "V",   CYAN,   BLACK, OSC_FONT_SIZE);
    y += INFO_ROW_H;

    /* Avg */
    lcd_fill(INFO_VAL_X1, y, INFO_VAL_X2, y + OSC_FONT_SIZE - 1, BLACK);
    osc_show_float(INFO_VX, y, g_osc.meas_vavg, 1, 2, WHITE,  BLACK, OSC_FONT_SIZE);
    osc_show_str  (INFO_UX, y, "V",   WHITE,  BLACK, OSC_FONT_SIZE);
    y += INFO_ROW_H;

    /* Frq（自动 Hz / kHz） */
    lcd_fill(INFO_VAL_X1, y, INFO_VAL_X2, y + OSC_FONT_SIZE - 1, BLACK);
    if (g_osc.meas_freq < 1000.0f) {
        osc_show_float(INFO_VX, y, g_osc.meas_freq, 3, 1, MAGENTA, BLACK, OSC_FONT_SIZE);
        osc_show_str  (INFO_UX, y, "Hz", MAGENTA, BLACK, OSC_FONT_SIZE);
    } else {
        osc_show_float(INFO_VX, y, g_osc.meas_freq / 1000.0f, 2, 2, MAGENTA, BLACK, OSC_FONT_SIZE);
        osc_show_str  (INFO_UX, y, "k",  MAGENTA, BLACK, OSC_FONT_SIZE);
    }
    y += INFO_ROW_H;

    /* Trg */
    lcd_fill(INFO_VAL_X1, y, INFO_VAL_X2, y + OSC_FONT_SIZE - 1, BLACK);
    osc_show_float(INFO_VX, y, g_osc.trig_level, 1, 2, YELLOW, BLACK, OSC_FONT_SIZE);
    osc_show_str  (INFO_UX, y, "V",   YELLOW, BLACK, OSC_FONT_SIZE);
}

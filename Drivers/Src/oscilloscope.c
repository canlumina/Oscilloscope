#include "oscilloscope.h"
#include "lcd.h"
#include "adc_sample.h"
#include <string.h>

/* lcd.h 没有的颜色，本文件局部补齐 */
#ifndef ORANGE
#define ORANGE      0xFD20
#endif
#define LIGHT_GRAY  LGRAY
#define DARK_GRAY   GRAY

#define OSC_FONT_SIZE  12

/* ============================================================
 * 电压量程表（满屏8格对应的总电压范围）
 * 默认档（idx=3）为 3.3V：每格 0.4125V，正好覆盖 STM32 GPIO 全幅
 * ============================================================ */
const float g_vscale_vpp[VSCALE_COUNT] = {
    0.5f,   /* 62.5 mV/div  小信号 */
    1.0f,   /* 125  mV/div  */
    2.0f,   /* 250  mV/div  */
    3.3f,   /* 412.5mV/div  STM32 GPIO 全幅（默认） */
    4.0f,   /* 500  mV/div  */
    8.0f,   /* 1    V/div   */
    16.0f,  /* 2    V/div   */
};

const char * const g_vscale_str[VSCALE_COUNT] = {
    "62.5mV", "125mV", "250mV", "3.3V ", "500mV", "1.00V", "2.00V",
};

OscState g_osc = {
    .vscale_idx   = 3,              /* 默认 3.3V 档（STM32 GPIO） */
    .timebase_idx = 3,              /* 默认 5ms/div */
    .trig_level   = 1.65f,
    .trig_mode    = TRIG_AUTO,
    .trig_edge    = TRIG_EDGE_RISE,
    .run          = 1,
    .single_done  = 0,
};

/* ============================================================
 * 字符串/浮点显示包装
 * ============================================================ */
static void osc_show_str(uint16_t x, uint16_t y, const char *s,
                         uint16_t fc, uint16_t bc, uint8_t size)
{
    uint32_t saved_bg = g_back_color;
    g_back_color = bc;
    lcd_show_string(x, y, 240, size, size, (char *)s, fc);
    g_back_color = saved_bg;
}

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

    char tmp[12];
    int  ti = 0;
    if (whole == 0) tmp[ti++] = '0';
    while (whole > 0) { tmp[ti++] = (char)('0' + whole % 10); whole /= 10; }
    if (neg) tmp[ti++] = '-';
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

/* 频率显示：固定 4 字符宽 24px。<1kHz 显示 "XXXH"；≥1kHz 显示 "X.Xk" 或 "XXk " */
static void osc_show_freq(uint16_t x, uint16_t y, float hz,
                          uint16_t fc, uint16_t bc)
{
    char buf[8];
    if (hz <= 0.0f) {
        osc_show_str(x, y, "----", fc, bc, OSC_FONT_SIZE);
        return;
    }
    if (hz < 1000.0f) {
        int n = (int)(hz + 0.5f);
        if (n > 999) n = 999;
        buf[0] = (n >= 100) ? ('0' + n / 100)        : ' ';
        buf[1] = (n >= 10)  ? ('0' + (n / 10) % 10)  : ((n >= 100) ? '0' : ' ');
        buf[2] = '0' + n % 10;
        buf[3] = 'H';
        buf[4] = '\0';
        osc_show_str(x, y, buf, fc, bc, OSC_FONT_SIZE);
    } else {
        float khz = hz / 1000.0f;
        if (khz > 99.9f) khz = 99.9f;
        if (khz < 10.0f) {
            osc_show_float(x, y, khz, 1, 1, fc, bc, OSC_FONT_SIZE);  /* "X.X" */
            osc_show_str(x + 18, y, "k", fc, bc, OSC_FONT_SIZE);
        } else {
            int n = (int)(khz + 0.5f);
            buf[0] = '0' + n / 10;
            buf[1] = '0' + n % 10;
            buf[2] = 'k';
            buf[3] = ' ';
            buf[4] = '\0';
            osc_show_str(x, y, buf, fc, bc, OSC_FONT_SIZE);
        }
    }
}

/* 脉宽显示：固定 4 字符宽。<1ms 显示 "XXXu"；≥1ms 显示 "X.Xm" 或 "XXms" */
static void osc_show_pulse(uint16_t x, uint16_t y, float us,
                           uint16_t fc, uint16_t bc)
{
    char buf[8];
    if (us < 0.0f) {
        osc_show_str(x, y, "----", fc, bc, OSC_FONT_SIZE);
        return;
    }
    if (us < 1000.0f) {
        int n = (int)(us + 0.5f);
        if (n > 999) n = 999;
        buf[0] = (n >= 100) ? ('0' + n / 100)       : ' ';
        buf[1] = (n >= 10)  ? ('0' + (n / 10) % 10) : ((n >= 100) ? '0' : ' ');
        buf[2] = '0' + n % 10;
        buf[3] = 'u';
        buf[4] = '\0';
        osc_show_str(x, y, buf, fc, bc, OSC_FONT_SIZE);
    } else {
        float ms = us / 1000.0f;
        if (ms > 99.0f) ms = 99.0f;
        if (ms < 10.0f) {
            osc_show_float(x, y, ms, 1, 1, fc, bc, OSC_FONT_SIZE);
            osc_show_str(x + 18, y, "m", fc, bc, OSC_FONT_SIZE);
        } else {
            int n = (int)(ms + 0.5f);
            buf[0] = '0' + n / 10;
            buf[1] = '0' + n % 10;
            buf[2] = 'm';
            buf[3] = 's';
            buf[4] = '\0';
            osc_show_str(x, y, buf, fc, bc, OSC_FONT_SIZE);
        }
    }
}

/* ============================================================
 * 增量绘制缓存
 * ============================================================ */
static uint16_t s_prev_ys1[OSC_WAVE_W];
static uint16_t s_prev_ys2[OSC_WAVE_W];
static uint8_t  s_has_prev    = 0;
static uint16_t s_prev_trig_y = 0;
static uint8_t  s_has_trig    = 0;

void Osc_InvalidateWave(void) { s_has_prev = 0; }

/* ============================================================
 * 清波形区 + 重画网格 + 重置增量缓存
 * 切换时基/量程时调用，避免旧波形残留
 * ============================================================ */
void Osc_ClearWaveArea(void)
{
    lcd_fill(OSC_WAVE_X + 1, OSC_WAVE_Y + 1,
             OSC_WAVE_X + OSC_WAVE_W - 2,
             OSC_WAVE_Y + OSC_WAVE_H - 2, BLACK);
    Osc_DrawGrid();
    s_has_prev = 0;
    s_has_trig = 0;     /* 触发线也要重画，否则缓存的 y 对应黑色像素已被擦 */
}

/* ============================================================
 * 双通道分屏映射
 *  上半屏 CH1: y=OSC_WAVE_Y..OSC_WAVE_Y+HALF-1 (16..111)
 *             0V → y=中线-1，vscale → y=屏顶
 *  下半屏 CH2: y=OSC_WAVE_Y+HALF..OSC_WAVE_Y+OSC_WAVE_H-1 (112..207)
 *             0V → y=屏底，vscale → y=中线
 *  量程语义：满屏(半屏)=vscale，0V 为基准（单极性，适合 GPIO 0~3.3V 信号）
 * ============================================================ */
#define OSC_HALF_H      (OSC_WAVE_H / 2)              /* 96 */
#define OSC_MID_Y       (OSC_WAVE_Y + OSC_HALF_H)     /* 112 中线 */

static uint16_t adc_to_y_ch1(uint16_t raw)
{
    float v      = ADC_Sample_ToVoltage(raw);
    float vscale = g_vscale_vpp[g_osc.vscale_idx];
    float ratio  = v / vscale;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    int16_t y = (int16_t)(OSC_MID_Y - 1 - ratio * (OSC_HALF_H - 1));
    if (y < OSC_WAVE_Y) y = OSC_WAVE_Y;
    if (y >= OSC_MID_Y) y = OSC_MID_Y - 1;
    return (uint16_t)y;
}

static uint16_t adc_to_y_ch2(uint16_t raw)
{
    float v      = ADC_Sample_ToVoltage(raw);
    float vscale = g_vscale_vpp[g_osc.vscale_idx];
    float ratio  = v / vscale;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    int16_t y = (int16_t)(OSC_WAVE_Y + OSC_WAVE_H - 1 - ratio * (OSC_HALF_H - 1));
    if (y < OSC_MID_Y) y = OSC_MID_Y;
    if (y >= OSC_WAVE_Y + OSC_WAVE_H) y = OSC_WAVE_Y + OSC_WAVE_H - 1;
    return (uint16_t)y;
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
    Osc_DrawInfoBarLabels();
    Osc_UpdateInfoBar();
    Osc_DrawTrigLine();
    Osc_DrawKeyHints();
}

/* ============================================================
 * 静态边框
 *  - 波形区边框
 *  - 状态栏底 / 按键栏顶 两条水平分隔
 *  - 右栏左边界一条竖线（只画到按键栏上方）
 * ============================================================ */
void Osc_DrawFrame(void)
{
    lcd_draw_rectangle(OSC_WAVE_X, OSC_WAVE_Y,
                       OSC_WAVE_X + OSC_WAVE_W - 1,
                       OSC_WAVE_Y + OSC_WAVE_H - 1, GRAY);
    lcd_draw_hline(0, OSC_WAVE_Y - 1, 320, GRAY);             /* 状态栏底 */
    lcd_draw_line(OSC_WAVE_W, 0, OSC_WAVE_W, OSC_KEYBAR_Y - 1, GRAY); /* 右栏左边界 */
    lcd_draw_hline(0, OSC_KEYBAR_Y, 320, GRAY);               /* 按键栏顶 */
}

/* ============================================================
 * 网格（双通道分屏：上半 4 格 / 中央分隔线 / 下半 4 格 / 10 列竖线）
 * ============================================================ */
void Osc_DrawGrid(void)
{
    uint16_t i, j;

    /* 垂直网格虚线（贯穿全屏，10 列）*/
    for (i = 1; i < OSC_GRID_COLS; i++) {
        uint16_t x = OSC_WAVE_X + i * OSC_CELL_W;
        for (j = OSC_WAVE_Y; j < OSC_WAVE_Y + OSC_WAVE_H; j += 4)
            lcd_draw_point(x, j, LGRAY);
    }

    /* 上半屏 CH1：3 条水平虚线（每格 OSC_HALF_H/4 = 24px） */
    for (i = 1; i < 4; i++) {
        uint16_t y = OSC_WAVE_Y + i * (OSC_HALF_H / 4);
        for (j = OSC_WAVE_X; j < OSC_WAVE_X + OSC_WAVE_W; j += 4)
            lcd_draw_point(j, y, LGRAY);
    }
    /* 下半屏 CH2：3 条水平虚线 */
    for (i = 1; i < 4; i++) {
        uint16_t y = OSC_MID_Y + i * (OSC_HALF_H / 4);
        for (j = OSC_WAVE_X; j < OSC_WAVE_X + OSC_WAVE_W; j += 4)
            lcd_draw_point(j, y, LGRAY);
    }

    /* 中央分隔线（CH1 的 0V / CH2 的 vscale 基准）— 实线 */
    lcd_draw_hline(OSC_WAVE_X, OSC_MID_Y - 1, OSC_WAVE_W, GRAY);

    /* 通道标识：在左边缘画一个小三角 + 文字（CH1/CH2 一目了然） */
    lcd_draw_point(OSC_WAVE_X + 2, OSC_WAVE_Y + OSC_HALF_H / 2,     GREEN);
    lcd_draw_point(OSC_WAVE_X + 2, OSC_WAVE_Y + OSC_HALF_H / 2 - 1, GREEN);
    lcd_draw_point(OSC_WAVE_X + 2, OSC_WAVE_Y + OSC_HALF_H / 2 + 1, GREEN);
    lcd_draw_point(OSC_WAVE_X + 2, OSC_MID_Y + OSC_HALF_H / 2,      CYAN);
    lcd_draw_point(OSC_WAVE_X + 2, OSC_MID_Y + OSC_HALF_H / 2 - 1,  CYAN);
    lcd_draw_point(OSC_WAVE_X + 2, OSC_MID_Y + OSC_HALF_H / 2 + 1,  CYAN);
}

/* ============================================================
 * 双通道波形增量绘制
 *  CH1=GREEN, CH2=CYAN
 * ============================================================ */
void Osc_DrawWave(uint32_t *adc_buf, uint16_t len)
{
    if (len < 2) return;

    uint16_t cnt = (len < OSC_WAVE_W) ? len : OSC_WAVE_W;

    if (s_has_prev) {
        for (uint16_t i = 1; i < cnt; i++) {
            lcd_draw_line((uint16_t)(OSC_WAVE_X + i - 1), s_prev_ys1[i - 1],
                          (uint16_t)(OSC_WAVE_X + i),     s_prev_ys1[i],     BLACK);
            lcd_draw_line((uint16_t)(OSC_WAVE_X + i - 1), s_prev_ys2[i - 1],
                          (uint16_t)(OSC_WAVE_X + i),     s_prev_ys2[i],     BLACK);
        }
    }

    s_prev_ys1[0] = adc_to_y_ch1(ADC_SAMPLE_CH1(adc_buf[0]));
    s_prev_ys2[0] = adc_to_y_ch2(ADC_SAMPLE_CH2(adc_buf[0]));
    for (uint16_t i = 1; i < cnt; i++) {
        s_prev_ys1[i] = adc_to_y_ch1(ADC_SAMPLE_CH1(adc_buf[i]));
        s_prev_ys2[i] = adc_to_y_ch2(ADC_SAMPLE_CH2(adc_buf[i]));
        lcd_draw_line((uint16_t)(OSC_WAVE_X + i - 1), s_prev_ys1[i - 1],
                      (uint16_t)(OSC_WAVE_X + i),     s_prev_ys1[i],     GREEN);
        lcd_draw_line((uint16_t)(OSC_WAVE_X + i - 1), s_prev_ys2[i - 1],
                      (uint16_t)(OSC_WAVE_X + i),     s_prev_ys2[i],     CYAN);
    }
    s_has_prev = 1;

    Osc_DrawTrigLine();
}

/* ============================================================
 * 触发电平虚线（只在 CH1 半屏内显示，用 CH1 映射）
 * ============================================================ */
void Osc_DrawTrigLine(void)
{
    /* 触发电平直接用 CH1 映射（单极性 0~vscale） */
    uint16_t trig_raw = (uint16_t)(g_osc.trig_level / 3.3f * 4095.0f);
    uint16_t y = adc_to_y_ch1(trig_raw);

    if (s_has_trig && s_prev_trig_y != y) {
        for (uint16_t x = OSC_WAVE_X; x < OSC_WAVE_X + OSC_WAVE_W; x += 6)
            lcd_draw_hline(x, s_prev_trig_y, 3, BLACK);
        lcd_draw_point(OSC_WAVE_X,     s_prev_trig_y,     BLACK);
        lcd_draw_point(OSC_WAVE_X + 1, s_prev_trig_y - 1, BLACK);
        lcd_draw_point(OSC_WAVE_X + 1, s_prev_trig_y + 1, BLACK);
    }

    for (uint16_t x = OSC_WAVE_X; x < OSC_WAVE_X + OSC_WAVE_W; x += 6)
        lcd_draw_hline(x, y, 3, YELLOW);
    lcd_draw_point(OSC_WAVE_X,     y,     YELLOW);
    lcd_draw_point(OSC_WAVE_X + 1, y - 1, YELLOW);
    lcd_draw_point(OSC_WAVE_X + 1, y + 1, YELLOW);

    s_prev_trig_y = y;
    s_has_trig    = 1;
}

/* ============================================================
 * 触发点查找（CH1）
 * ============================================================ */
uint16_t Osc_FindTrigger(uint32_t *adc_buf, uint16_t len)
{
    uint16_t trig_raw = (uint16_t)(g_osc.trig_level / 3.3f * 4095.0f);
    if (len <= OSC_WAVE_W) return 0;
    if (g_osc.trig_edge == TRIG_EDGE_RISE) {
        for (uint16_t i = 1; i < len - OSC_WAVE_W; i++)
            if (ADC_SAMPLE_CH1(adc_buf[i - 1]) <  trig_raw &&
                ADC_SAMPLE_CH1(adc_buf[i])     >= trig_raw) return i;
    } else {
        for (uint16_t i = 1; i < len - OSC_WAVE_W; i++)
            if (ADC_SAMPLE_CH1(adc_buf[i - 1]) >  trig_raw &&
                ADC_SAMPLE_CH1(adc_buf[i])     <= trig_raw) return i;
    }
    return 0;
}

/* ============================================================
 * 单通道测量（Vpp/Max/Min/Avg + Freq + Thi + Tlo）
 *  阈值用 (vmax+vmin)/2，比 vavg 对非对称占空比更鲁棒。
 *  Thi/Tlo: 第一个上升沿到第一个下降沿 = 高电平宽度，
 *           第一个下降沿到第二个上升沿 = 低电平宽度。
 * ============================================================ */
typedef uint16_t (*sample_get_fn)(uint32_t w);
static uint16_t get_ch1_sample(uint32_t w) { return ADC_SAMPLE_CH1(w); }
static uint16_t get_ch2_sample(uint32_t w) { return ADC_SAMPLE_CH2(w); }

static void measure_one(uint32_t *buf, uint16_t len, sample_get_fn get,
                        OscChannelMeas *m, float sample_us)
{
    if (len == 0) return;

    uint32_t sum  = 0;
    uint16_t vmax = 0, vmin = 4095;
    for (uint16_t i = 0; i < len; i++) {
        uint16_t v = get(buf[i]);
        sum += v;
        if (v > vmax) vmax = v;
        if (v < vmin) vmin = v;
    }
    m->vmax = ADC_Sample_ToVoltage(vmax);
    m->vmin = ADC_Sample_ToVoltage(vmin);
    m->vpp  = m->vmax - m->vmin;
    m->vavg = ADC_Sample_ToVoltage((uint16_t)(sum / len));

    /* 信号摆幅过小（< ~24mV）认作直流，频率/脉宽都标无效 */
    if ((vmax - vmin) < 30) {
        m->freq      = 0.0f;
        m->t_high_us = -1.0f;
        m->t_low_us  = -1.0f;
        return;
    }

    uint16_t mid_raw = (uint16_t)(((uint32_t)vmax + (uint32_t)vmin) / 2);

    int32_t idx_r1 = -1, idx_f1 = -1, idx_r2 = -1;
    for (uint16_t i = 1; i < len; i++) {
        uint16_t prev = get(buf[i - 1]);
        uint16_t cur  = get(buf[i]);
        if (prev < mid_raw && cur >= mid_raw) {              /* 上升沿 */
            if (idx_r1 < 0)       idx_r1 = i;
            else if (idx_f1 > 0)  { idx_r2 = i; break; }
        } else if (prev > mid_raw && cur <= mid_raw) {       /* 下降沿 */
            if (idx_r1 >= 0 && idx_f1 < 0) idx_f1 = i;
        }
    }

    m->freq      = (idx_r1 >= 0 && idx_r2 > idx_r1)
                   ? 1000000.0f / ((idx_r2 - idx_r1) * sample_us) : 0.0f;
    m->t_high_us = (idx_r1 >= 0 && idx_f1 > idx_r1) ? (idx_f1 - idx_r1) * sample_us : -1.0f;
    m->t_low_us  = (idx_f1 >= 0 && idx_r2 > idx_f1) ? (idx_r2 - idx_f1) * sample_us : -1.0f;
}

void Osc_Measure(uint32_t *adc_buf, uint16_t len)
{
    /* TIM3 PSC=71 → 1MHz 计数，采样间隔 = ARR+1 µs */
    float sample_us = (float)(TIM3->ARR + 1);
    measure_one(adc_buf, len, get_ch1_sample, &g_osc.ch1, sample_us);
    measure_one(adc_buf, len, get_ch2_sample, &g_osc.ch2, sample_us);
}

/* ============================================================
 * 状态栏（顶部 0..319 × 0..15）
 * ============================================================ */
void Osc_UpdateStatusBar(void)
{
    lcd_fill(0, 0, 319, 15, BLACK);

    osc_show_str(0,   0, g_timebase_str[g_osc.timebase_idx], CYAN,   BLACK, OSC_FONT_SIZE);
    osc_show_str(72,  0, g_vscale_str[g_osc.vscale_idx],     YELLOW, BLACK, OSC_FONT_SIZE);
    {
        static const char * const tmode[] = {"AUTO", "NORM", "SING"};
        osc_show_str(132, 0, tmode[g_osc.trig_mode], ORANGE, BLACK, OSC_FONT_SIZE);
    }
    osc_show_str(168, 0, (g_osc.trig_edge == TRIG_EDGE_RISE) ? "/\\" : "\\/",
                 WHITE, BLACK, OSC_FONT_SIZE);
    osc_show_str(198, 0, g_osc.run ? "RUN" : "STP",
                 g_osc.run ? GREEN : RED, BLACK, OSC_FONT_SIZE);

    osc_show_str(244, 0, "CH1", GREEN,      BLACK, OSC_FONT_SIZE);
    osc_show_str(264, 0, "+",   LIGHT_GRAY, BLACK, OSC_FONT_SIZE);
    osc_show_str(272, 0, "CH2", CYAN,       BLACK, OSC_FONT_SIZE);

    /* 状态栏清屏后右栏左边界顶端那段要补回 */
    lcd_draw_line(OSC_WAVE_W, 0, OSC_WAVE_W, 15, GRAY);
}

/* ============================================================
 * 右栏（240..319 × 16..207）— 三列布局
 *  标签列(灰) | CH1 数值(绿) | CH2 数值(青)
 *
 *  x=242: 标签起点（3 char = 18px）
 *  x=266: CH1 数值（4 char = 24px）
 *  x=294: CH2 数值（4 char = 24px）
 *  y=20 起，每行 24px 间距，共 8 行 = 192px → 占满右栏高度
 * ============================================================ */
#define INFO_LX     242
#define INFO_C1X    266
#define INFO_C2X    294
#define INFO_ROW_H  24
#define INFO_Y0     20
#define INFO_ROWS   8

static const char * const s_info_labels[INFO_ROWS] = {
    "Vpp", "Max", "Min", "Avg", "Frq", "Thi", "Tlo", "Trg",
};

void Osc_DrawInfoBarLabels(void)
{
    for (int i = 0; i < INFO_ROWS; i++)
        osc_show_str(INFO_LX, INFO_Y0 + i * INFO_ROW_H,
                     (char *)s_info_labels[i],
                     LIGHT_GRAY, BLACK, OSC_FONT_SIZE);
}

/* 24×12 像素的数值字段先填黑再写新值 */
static inline void clear_value(uint16_t x, uint16_t y)
{
    lcd_fill(x, y, x + 23, y + OSC_FONT_SIZE - 1, BLACK);
}

void Osc_UpdateInfoBar(void)
{
    OscChannelMeas *c1 = &g_osc.ch1;
    OscChannelMeas *c2 = &g_osc.ch2;
    uint16_t y;

    /* Vpp */
    y = INFO_Y0 + 0 * INFO_ROW_H;
    clear_value(INFO_C1X, y); clear_value(INFO_C2X, y);
    osc_show_float(INFO_C1X, y, c1->vpp,  1, 2, GREEN, BLACK, OSC_FONT_SIZE);
    osc_show_float(INFO_C2X, y, c2->vpp,  1, 2, CYAN,  BLACK, OSC_FONT_SIZE);

    /* Max */
    y = INFO_Y0 + 1 * INFO_ROW_H;
    clear_value(INFO_C1X, y); clear_value(INFO_C2X, y);
    osc_show_float(INFO_C1X, y, c1->vmax, 1, 2, GREEN, BLACK, OSC_FONT_SIZE);
    osc_show_float(INFO_C2X, y, c2->vmax, 1, 2, CYAN,  BLACK, OSC_FONT_SIZE);

    /* Min */
    y = INFO_Y0 + 2 * INFO_ROW_H;
    clear_value(INFO_C1X, y); clear_value(INFO_C2X, y);
    osc_show_float(INFO_C1X, y, c1->vmin, 1, 2, GREEN, BLACK, OSC_FONT_SIZE);
    osc_show_float(INFO_C2X, y, c2->vmin, 1, 2, CYAN,  BLACK, OSC_FONT_SIZE);

    /* Avg */
    y = INFO_Y0 + 3 * INFO_ROW_H;
    clear_value(INFO_C1X, y); clear_value(INFO_C2X, y);
    osc_show_float(INFO_C1X, y, c1->vavg, 1, 2, GREEN, BLACK, OSC_FONT_SIZE);
    osc_show_float(INFO_C2X, y, c2->vavg, 1, 2, CYAN,  BLACK, OSC_FONT_SIZE);

    /* Frq */
    y = INFO_Y0 + 4 * INFO_ROW_H;
    clear_value(INFO_C1X, y); clear_value(INFO_C2X, y);
    osc_show_freq(INFO_C1X, y, c1->freq, GREEN, BLACK);
    osc_show_freq(INFO_C2X, y, c2->freq, CYAN,  BLACK);

    /* Thi */
    y = INFO_Y0 + 5 * INFO_ROW_H;
    clear_value(INFO_C1X, y); clear_value(INFO_C2X, y);
    osc_show_pulse(INFO_C1X, y, c1->t_high_us, GREEN, BLACK);
    osc_show_pulse(INFO_C2X, y, c2->t_high_us, CYAN,  BLACK);

    /* Tlo */
    y = INFO_Y0 + 6 * INFO_ROW_H;
    clear_value(INFO_C1X, y); clear_value(INFO_C2X, y);
    osc_show_pulse(INFO_C1X, y, c1->t_low_us, GREEN, BLACK);
    osc_show_pulse(INFO_C2X, y, c2->t_low_us, CYAN,  BLACK);

    /* Trg：只 CH1 列显示 */
    y = INFO_Y0 + 7 * INFO_ROW_H;
    clear_value(INFO_C1X, y); clear_value(INFO_C2X, y);
    osc_show_float(INFO_C1X, y, g_osc.trig_level, 1, 2, YELLOW, BLACK, OSC_FONT_SIZE);
}

/* ============================================================
 * 底部按键提示栏（y=208..239，32px 高，只画一次）
 *  K0=时基  K1=量程  K2=Run/Stop
 * ============================================================ */
void Osc_DrawKeyHints(void)
{
    uint16_t y = OSC_KEYBAR_Y + (OSC_KEYBAR_H - OSC_FONT_SIZE) / 2;
    osc_show_str( 10, y, "K0:Tbase",   LIGHT_GRAY, BLACK, OSC_FONT_SIZE);
    osc_show_str(110, y, "K1:Vscale",  LIGHT_GRAY, BLACK, OSC_FONT_SIZE);
    osc_show_str(220, y, "K2:Run/Stp", LIGHT_GRAY, BLACK, OSC_FONT_SIZE);
}

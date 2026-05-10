/* ============================================================
 * 简易示波器 主程序（双通道版）
 * 开发板: 德飞莱 超翼M35 (STM32F103ZET6)
 * LCD:    2.8寸 ILI9325 (FSMC驱动，横屏 320×240)
 * ============================================================
 *
 * 【接线说明】
 *  CH1 信号输入: PA0  (ADC1_IN0，0~3.3V，必须加保护电路！)
 *  CH2 信号输入: PA1  (ADC2_IN1，0~3.3V，必须加保护电路！)
 *  LCD:          通过FSMC接口，使用板载TFT-LCD排母
 *  按键（三键方案）：
 *    KEY0 (PE4) - 时基切换 (→ 更慢)
 *    KEY1 (PE3) - 电压量程切换（双通道共用）
 *    KEY2 (PE2) - Run / Stop 切换
 *
 * 【双通道说明】
 *  ADC1+ADC2 同步规则采样（DUAL REGSIMULT），每点严格对齐。
 *  打包样本: 低16位=CH1，高16位=CH2
 *  显示: CH1=绿色 GREEN，CH2=青色 CYAN
 *  测量栏: 只测 CH1；CH2 仅显示波形供相位/相对幅值参考
 *  触发: 以 CH1 为基准
 * ============================================================ */

#include "main.h"
#include "adc_sample.h"
#include "oscilloscope.h"
#include "button.h"
#include <string.h>

static uint32_t s_disp_buf[SAMPLE_BUF_SIZE];   /* CH1+CH2 打包样本 */

/* ============================================================
 * 按键处理（三键方案）
 * ============================================================ */
static void Handle_Key(uint8_t key)
{
    switch (key) {
    case KEY0_PRES: /* 时基切换 */
        g_osc.timebase_idx++;
        if (g_osc.timebase_idx >= TIMEBASE_COUNT)
            g_osc.timebase_idx = 0;
        ADC_Sample_SetTimebase(g_osc.timebase_idx);
        Osc_UpdateStatusBar();
        break;

    case KEY1_PRES: /* 量程切换：触发线位置和波形映射都会变 */
        g_osc.vscale_idx++;
        if (g_osc.vscale_idx >= VSCALE_COUNT)
            g_osc.vscale_idx = 0;
        Osc_InvalidateWave();   /* 缓存的 prev_ys 已不可用 */
        Osc_UpdateStatusBar();
        Osc_DrawTrigLine();     /* 量程变了 → 触发线 y 变了 */
        break;

    case KEY2_PRES: /* Run / Stop */
        g_osc.run = !g_osc.run;
        Osc_UpdateStatusBar();
        break;

    default:
        break;
    }
}

/* ============================================================
 * 等待 DMA 采样完成（带超时，期间继续扫按键）
 * 不能用 HAL_GetTick：项目的 delay_init 关闭了 SysTick 中断，
 * 转用 delay_ms(1) 自累计；button_scan 在无按键时不阻塞。
 * ============================================================ */
static uint8_t Wait_Sample_Done(uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    while (!g_sample_done) {
        uint8_t k = button_scan(0);
        if (k) Handle_Key(k);
        delay_ms(1);
        if (++elapsed >= timeout_ms) return 0;
    }
    return 1;
}

/* ============================================================
 * 主循环
 * ============================================================ */
int main(void)
{
    HAL_Init();
    system_clock_init(RCC_PLL_MUL9);   /* HSE 8MHz × 9 = 72MHz */
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_2);
    delay_init(72);

    u1_init(115200);                   /* lcd_init 内部会用 u1_printf 打印 LCD ID */
    lcd_init();
    lcd_display_dir(1);                /* 切横屏 320×240 */
    button_init();
    ADC_Sample_Init();
    ADC_Sample_SetTimebase(g_osc.timebase_idx);

    Osc_Init(); /* 初始化界面 */

    while (1) {
        uint8_t key = button_scan(0);
        if (key) Handle_Key(key);

        /* 停止模式下跳过采样 */
        if (!g_osc.run) {
            delay_ms(50);
            continue;
        }

        /* ---- 开始采样 ---- */
        ADC_Sample_Start();

        /* 等待最大 500ms（慢时基下一帧时间较长） */
        if (!Wait_Sample_Done(500)) {
            /* 超时：AUTO 模式下也直接画当前缓冲，其余跳过 */
            if (g_osc.trig_mode != TRIG_AUTO) continue;
        }

        /* ---- 触发处理 ---- */
        uint16_t offset = 0;
        if (g_osc.trig_mode != TRIG_AUTO) {
            offset = Osc_FindTrigger(g_adc_buf, SAMPLE_BUF_SIZE);
        }

        /* 取显示数据（从触发点开始，长度为屏宽） */
        uint16_t avail    = SAMPLE_BUF_SIZE - offset;
        uint16_t copy_len = (avail < SAMPLE_BUF_SIZE) ? avail : SAMPLE_BUF_SIZE;
        memcpy(s_disp_buf, g_adc_buf + offset, copy_len * sizeof(uint32_t));

        /* ---- 测量 + 绘制（StatusBar 不每帧刷新，避免闪烁） ---- */
        Osc_Measure(s_disp_buf, copy_len);
        Osc_DrawWave(s_disp_buf, copy_len);
        Osc_UpdateInfoBar();
    }
}

#ifndef __ADC_SAMPLE_H
#define __ADC_SAMPLE_H

#include "stm32f1xx.h"

/* ============================================================
 * 双通道同步采样配置（ADC1+ADC2 Regular Simultaneous Mode）
 *  CH1 -> ADC1_IN0 (PA0)
 *  CH2 -> ADC2_IN1 (PA1)
 *
 *  TIM3_TRGO 同时触发两个 ADC，每个采样点严格对齐。
 *  DMA 走 ADC1 端，每次搬一个 32-bit 字：
 *      bit  0..15 = ADC1 采样值 (CH1)
 *      bit 16..31 = ADC2 采样值 (CH2)
 *
 *  注意：PA0 / PA1 都仅接 0~3.3V，必须外加分压保护电路。
 * ============================================================ */

#define SAMPLE_ADC1             ADC1
#define SAMPLE_ADC2             ADC2
#define SAMPLE_ADC1_CHANNEL     ADC_CHANNEL_0
#define SAMPLE_ADC2_CHANNEL     ADC_CHANNEL_1
#define SAMPLE_GPIO_PORT        GPIOA
#define SAMPLE_GPIO_PIN_CH1     GPIO_PIN_0
#define SAMPLE_GPIO_PIN_CH2     GPIO_PIN_1
#define SAMPLE_DMA_CHANNEL      DMA1_Channel1

/* 采样缓冲区大小（每屏列数） */
#define SAMPLE_BUF_SIZE         240

/* 时基档位数量 */
#define TIMEBASE_COUNT          9

/* 采样完成标志 */
extern volatile uint8_t  g_sample_done;
/* 采样原始数据：每个 word 包含 CH1(低16) + CH2(高16) */
extern uint32_t          g_adc_buf[SAMPLE_BUF_SIZE];

/* 时基描述字符串 */
extern const char * const g_timebase_str[TIMEBASE_COUNT];

/* 从打包样本里取出单通道原始值 */
#define ADC_SAMPLE_CH1(w)       ((uint16_t)((w) & 0xFFFFu))
#define ADC_SAMPLE_CH2(w)       ((uint16_t)((w) >> 16))

/* 函数声明 */
void  ADC_Sample_Init(void);
void  ADC_Sample_SetTimebase(uint8_t idx);
uint8_t ADC_Sample_GetTimebaseIdx(void);
void  ADC_Sample_Start(void);
float ADC_Sample_ToVoltage(uint16_t raw);  /* raw -> 实际电压(V) */

#endif /* __ADC_SAMPLE_H */

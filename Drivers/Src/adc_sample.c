#include "adc_sample.h"

/* ============================================================
 * 时基档位表（PSC=71 → TIM3 计数 1MHz，每 tick=1µs）
 * 双重模式下两 ADC 并行采样，每点 ADC1+ADC2 同时刻取值。
 *
 * idx 每像素   每格    TIM3 ARR  备注
 *  0  2µs     48µs    1         接近 F103 ADC 极限（~1.17µs/转换）
 *  1  4µs     96µs    3
 *  2  8µs     192µs   7
 *  3  21µs    504µs   20        默认（适合 PWM 观察）
 *  4  42µs    1ms     41
 *  5  84µs    2ms     83
 *  6  209µs   5ms     208
 *  7  417µs   10ms    416
 *  8  834µs   20ms    833
 *  9  2.08ms  50ms    2083
 * 10  4.17ms  100ms   4166
 * 11  8.33ms  200ms   8333
 *
 * 0 号档 ARR=1 时 sample 间隔=2µs，刚好高于 ADCCLK=12MHz 下 12bit
 * 单次转换最低 14 cycles ≈ 1.17µs，可用但留余量小。
 * ============================================================ */
static const uint16_t s_tim_arr[TIMEBASE_COUNT] = {
    1, 3, 7, 20, 41, 83, 208, 416, 833, 2083, 4166, 8333,
};

const char * const g_timebase_str[TIMEBASE_COUNT] = {
    "50us/d ", "100us/d", "200us/d",
    "500us/d", "1ms/div", "2ms/div",
    "5ms/div", "10ms/d ", "20ms/d ",
    "50ms/d ", "100ms/d", "200ms/d",
};

volatile uint8_t g_sample_done = 0;
uint32_t         g_adc_buf[SAMPLE_BUF_SIZE];   /* 每字 = (CH2<<16) | CH1 */

static uint8_t   s_timebase_idx = 3;  /* 默认5ms/div */

static ADC_HandleTypeDef hadc1;
static ADC_HandleTypeDef hadc2;
static DMA_HandleTypeDef hdma_adc1;
static TIM_HandleTypeDef htim3;

/* ============================================================
 * DMA1_Channel1 中断 — 转交 HAL，走 HAL_ADC_ConvCpltCallback
 * ============================================================ */
void DMA1_Channel1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_adc1);
}

/* ============================================================
 * 双重模式 DMA 完成回调
 *  - 停 TIM3 触发
 *  - 停 ADC1+ADC2 + DMA（HAL_ADCEx_MultiModeStop_DMA 一并处理）
 *  - 置位 g_sample_done
 * ============================================================ */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == SAMPLE_ADC1) {
        HAL_TIM_Base_Stop(&htim3);
        HAL_ADCEx_MultiModeStop_DMA(hadc);
        g_sample_done = 1;
    }
}

/* ============================================================
 * 初始化：ADC1 + ADC2 双重规则同步 + DMA 32-bit + TIM3 TRGO
 * ============================================================ */
void ADC_Sample_Init(void)
{
    GPIO_InitTypeDef        gpio_init  = {0};
    ADC_ChannelConfTypeDef  ch_cfg     = {0};
    ADC_MultiModeTypeDef    multimode  = {0};
    TIM_MasterConfigTypeDef tim_master = {0};

    /* ---- 时钟 ---- */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_ADC2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* ADCCLK = PCLK2 / 6 = 12MHz（F103 上限 14MHz） */
    __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);

    /* ---- GPIO: PA0 / PA1 模拟输入 ---- */
    gpio_init.Pin  = SAMPLE_GPIO_PIN_CH1 | SAMPLE_GPIO_PIN_CH2;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SAMPLE_GPIO_PORT, &gpio_init);

    /* ---- DMA1 Channel1 (ADC1)，双重模式下搬 32-bit 字 ---- */
    hdma_adc1.Instance                 = SAMPLE_DMA_CHANNEL;
    hdma_adc1.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;   /* 双重模式 DR=32bit */
    hdma_adc1.Init.MemDataAlignment    = DMA_MDATAALIGN_WORD;
    hdma_adc1.Init.Mode                = DMA_NORMAL;            /* 采满即停 */
    hdma_adc1.Init.Priority            = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&hdma_adc1);

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    /* ---- ADC1 (Master): T3_TRGO 触发，DMA 通道源 ---- */
    hadc1.Instance                   = SAMPLE_ADC1;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.NbrOfDiscConversion   = 0;
    hadc1.Init.ExternalTrigConv      = ADC_EXTERNALTRIGCONV_T3_TRGO;
    HAL_ADC_Init(&hadc1);

    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc1);

    ch_cfg.Channel      = SAMPLE_ADC1_CHANNEL;
    ch_cfg.Rank         = ADC_REGULAR_RANK_1;
    ch_cfg.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    HAL_ADC_ConfigChannel(&hadc1, &ch_cfg);

    /* ---- ADC2 (Slave): 软件触发占位，由 master 同步 ---- */
    hadc2.Instance                   = SAMPLE_ADC2;
    hadc2.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc2.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc2.Init.ContinuousConvMode    = DISABLE;
    hadc2.Init.NbrOfConversion       = 1;
    hadc2.Init.DiscontinuousConvMode = DISABLE;
    hadc2.Init.NbrOfDiscConversion   = 0;
    hadc2.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    HAL_ADC_Init(&hadc2);

    ch_cfg.Channel      = SAMPLE_ADC2_CHANNEL;
    ch_cfg.Rank         = ADC_REGULAR_RANK_1;
    ch_cfg.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    HAL_ADC_ConfigChannel(&hadc2, &ch_cfg);

    /* ---- 启用 ADC1+ADC2 双重规则同步采样 ---- */
    multimode.Mode = ADC_DUALMODE_REGSIMULT;
    HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode);

    /* ---- 自校准：两个 ADC 都需要 ---- */
    HAL_ADCEx_Calibration_Start(&hadc1);
    HAL_ADCEx_Calibration_Start(&hadc2);

    /* F1 HAL 已知坑：HAL_ADC_Init 不写 CR2.EXTTRIG（自注释说放在 Start_xxx 里写），
     * 但 HAL_ADCEx_MultiModeStart_DMA 只为 master(ADC1) 写 EXTTRIG，不写 slave(ADC2)。
     * 结果 dual REGSIMULT 下 slave 的硬件触发通路未使能，DR 高 16 位永远是 0。
     * 此处手动补一刀。参考 RM0008 §11.12.3 ADC_CR2.EXTTRIG */
    SET_BIT(hadc2.Instance->CR2, ADC_CR2_EXTTRIG);

    /* ---- TIM3: 1MHz 计数，TRGO=Update（同时触发两个 ADC） ---- */
    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 71;                          /* 72MHz/72 = 1MHz */
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = s_tim_arr[s_timebase_idx];
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_Base_Init(&htim3);

    tim_master.MasterOutputTrigger = TIM_TRGO_UPDATE;
    tim_master.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim3, &tim_master);
}

/* ============================================================
 * 设置时基（即时改 TIM3 重载值）
 * ============================================================ */
void ADC_Sample_SetTimebase(uint8_t idx)
{
    if (idx >= TIMEBASE_COUNT) idx = TIMEBASE_COUNT - 1;
    s_timebase_idx = idx;
    __HAL_TIM_SET_AUTORELOAD(&htim3, s_tim_arr[idx]);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
}

uint8_t ADC_Sample_GetTimebaseIdx(void)
{
    return s_timebase_idx;
}

/* ============================================================
 * 启动一次采样：双重模式 DMA + TIM3 触发
 *
 * F1 HAL 已知坑：HAL_ADCEx_MultiModeStop_DMA 内部会
 *   CLEAR_BIT(ADC1->CR1, ADC_CR1_DUALMOD)
 * 把双重模式位清零，下次启动会退化为独立模式（ADC1.DR 高 16 位永远 0）。
 * 复位后第一次采样工作正常，回调里 stop 后 DUALMOD 被清，第 2 次起 CH2 失效。
 * 修法：每次启动前手动写回 REGSIMULT。此刻两个 ADC 都 disabled，写位合法。
 * 参考 RM0008 §11.12.2 ADC_CR1.DUALMOD
 * ============================================================ */
void ADC_Sample_Start(void)
{
    g_sample_done = 0;

    MODIFY_REG(hadc1.Instance->CR1, ADC_CR1_DUALMOD, ADC_DUALMODE_REGSIMULT);

    HAL_ADCEx_MultiModeStart_DMA(&hadc1, g_adc_buf, SAMPLE_BUF_SIZE);
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    HAL_TIM_Base_Start(&htim3);
}

/* ============================================================
 * 原始值转电压
 * ============================================================ */
float ADC_Sample_ToVoltage(uint16_t raw)
{
    return (float)raw * 3.3f / 4095.0f;
}

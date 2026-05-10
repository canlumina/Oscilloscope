#include "timer.h"

TIM_HandleTypeDef tim3;
TIM_OC_InitTypeDef tim3_oc3_pwm;

void Timer3_Init(uint16_t arr, uint16_t psc)
{
    tim3.Instance = TIM3;
    tim3.Init.Prescaler = psc;
    tim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim3.Init.Period = arr;
    tim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV2;
    tim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_PWM_Init(&tim3);
    __HAL_TIM_CLEAR_FLAG(&tim3, TIM_FLAG_UPDATE);

    tim3_oc3_pwm.OCMode = TIM_OCMODE_PWM1;
    tim3_oc3_pwm.Pulse = arr / 2;
    tim3_oc3_pwm.OCPolarity = TIM_OCPOLARITY_LOW;
    tim3_oc3_pwm.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&tim3, &tim3_oc3_pwm, TIM_CHANNEL_3);

    HAL_TIM_PWM_Start(&tim3, TIM_CHANNEL_3);
}

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    GPIO_InitTypeDef GPIO_InitType;
    if (htim->Instance == TIM3)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_TIM3_CLK_ENABLE();

        GPIO_InitType.Pin = GPIO_PIN_0;
        GPIO_InitType.Mode = GPIO_MODE_AF_PP;
        GPIO_InitType.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &GPIO_InitType);
    }
}

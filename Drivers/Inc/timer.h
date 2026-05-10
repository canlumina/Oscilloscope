#ifndef __TIMER_H_
#define __TIMER_H_
#include "stm32f1xx_hal.h"

extern TIM_HandleTypeDef tim3;
extern TIM_OC_InitTypeDef tim3_oc3_pwm;
void Timer3_Init(uint16_t arr, uint16_t psc);

#endif

#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f1xx.h"

#define SYS_SUPPORT_OS 0


void delay_init(uint16_t sysclk);
void delay_us(uint32_t nus);
void delay_ms(uint16_t nms);

#endif

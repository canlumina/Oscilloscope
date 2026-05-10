#ifndef _LED_H
#define _LED_H
#include "stm32f1xx_hal.h"
/*
 * 部分：LED定义
 * 描述：此部分定义了LED相关的GPIO端口、引脚及时钟使能宏。
 */
#define LED0_GPIO_PORT GPIOB
#define LED0_GPIO_PIN GPIO_PIN_5
#define LED0_GPIO_CLK_ENABLE()                                                                                                                                                     \
    do {                                                                                                                                                                           \
        __HAL_RCC_GPIOB_CLK_ENABLE();                                                                                                                                              \
    } while (0) /* PB端口时钟使能 */

#define LED1_GPIO_PORT GPIOE
#define LED1_GPIO_PIN GPIO_PIN_5
#define LED1_GPIO_CLK_ENABLE()                                                                                                                                                     \
    do {                                                                                                                                                                           \
        __HAL_RCC_GPIOE_CLK_ENABLE();                                                                                                                                              \
    } while (0) /* PE端口时钟使能 */

/*
 * 部分：LED操作宏
 * 描述：此部分定义了LED的开、关及闪烁操作的宏。
 */
#define LED0(x)                                                                                                                                                                    \
    do {                                                                                                                                                                           \
        x ? HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_SET) : HAL_GPIO_WritePin(LED0_GPIO_PORT, LED0_GPIO_PIN, GPIO_PIN_RESET);                                     \
    } while (0) /* 控制LED0的开与关 */

#define LED1(x)                                                                                                                                                                    \
    do {                                                                                                                                                                           \
        x ? HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_SET) : HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_RESET);                                     \
    } while (0) /* 控制LED1的开与关 */

/* LED翻转宏 */
#define LED0_TOGGLE()                                                                                                                                                              \
    do {                                                                                                                                                                           \
        HAL_GPIO_TogglePin(LED0_GPIO_PORT, LED0_GPIO_PIN);                                                                                                                         \
    } while (0) /* 翻转LED0的状态 */
#define LED1_TOGGLE()                                                                                                                                                              \
    do {                                                                                                                                                                           \
        HAL_GPIO_TogglePin(LED1_GPIO_PORT, LED1_GPIO_PIN);                                                                                                                         \
    } while (0) /* 翻转LED1的状态 */

/*
 * 部分：LED初始化函数
 * 描述：初始化LED相关的GPIO端口，为使用LED做准备。
 */
void led_init(void); /* LED初始化函数 */

#endif

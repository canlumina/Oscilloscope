#ifndef __KEY_H
#define __KEY_H
#include "stm32f1xx.h"

// 键盘配置定义
#define KEY0_GPIO_PORT GPIOE     // KEY0使用的GPIO端口
#define KEY0_GPIO_PIN GPIO_PIN_4 // KEY0使用的GPIO引脚
#define KEY0_GPIO_CLK_ENABLE()                                                                                                                                                     \
    do {                                                                                                                                                                           \
        __HAL_RCC_GPIOE_CLK_ENABLE();                                                                                                                                              \
    } while (0) /* 启用KEY0 GPIO的时钟 */

#define KEY1_GPIO_PORT GPIOE     // KEY1使用的GPIO端口
#define KEY1_GPIO_PIN GPIO_PIN_3 // KEY1使用的GPIO引脚
#define KEY1_GPIO_CLK_ENABLE()                                                                                                                                                     \
    do {                                                                                                                                                                           \
        __HAL_RCC_GPIOE_CLK_ENABLE();                                                                                                                                              \
    } while (0) /* 启用KEY1 GPIO的时钟 */

#define KEY2_GPIO_PORT GPIOE     // KEY2使用的GPIO端口
#define KEY2_GPIO_PIN GPIO_PIN_2 // KEY2使用的GPIO引脚
#define KEY2_GPIO_CLK_ENABLE()                                                                                                                                                     \
    do {                                                                                                                                                                           \
        __HAL_RCC_GPIOE_CLK_ENABLE();                                                                                                                                              \
    } while (0) /* 启用KEY2 GPIO的时钟 */

#define WKUP_GPIO_PORT GPIOA     // WKUP使用的GPIO端口
#define WKUP_GPIO_PIN GPIO_PIN_0 // WKUP使用的GPIO引脚
#define WKUP_GPIO_CLK_ENABLE()                                                                                                                                                     \
    do {                                                                                                                                                                           \
        __HAL_RCC_GPIOA_CLK_ENABLE();                                                                                                                                              \
    } while (0) /* 启用WKUP GPIO的时钟 */

// 键盘输入状态定义
#define KEY0 HAL_GPIO_ReadPin(KEY0_GPIO_PORT, KEY0_GPIO_PIN)  /* 读取KEY0的状态 */
#define KEY1 HAL_GPIO_ReadPin(KEY1_GPIO_PORT, KEY1_GPIO_PIN)  /* 读取KEY1的状态 */
#define KEY2 HAL_GPIO_ReadPin(KEY2_GPIO_PORT, KEY2_GPIO_PIN)  /* 读取KEY2的状态 */
#define WK_UP HAL_GPIO_ReadPin(WKUP_GPIO_PORT, WKUP_GPIO_PIN) /* 读取WKUP的状态 */

// 键盘按键枚举
#define KEY0_PRES 1 /* KEY0按键 */
#define KEY1_PRES 2 /* KEY1按键 */
#define KEY2_PRES 3 /* KEY2按键 */
#define WKUP_PRES 4 /* WKUP按键(即唤醒键) */

// 键盘初始化函数
void button_init(void); /* 初始化键盘 */
// 键盘扫描函数
// mode参数：按键扫描模式
// 返回值：按下按键的枚举值
uint8_t button_scan(uint8_t mode);

#endif

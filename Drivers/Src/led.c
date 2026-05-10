#include "led.h"

/**
 * @brief 初始化LED相关的GPIO端口，为使用LED做好准备
 * @param 无
 * @retval 无
 */
void led_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    // 启用LED0的时钟
    LED0_GPIO_CLK_ENABLE();
    // 启用LED1的时钟
    LED1_GPIO_CLK_ENABLE();

    // 配置LED0的GPIO初始化结构体
    gpio_init_struct.Pin = LED0_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    // 初始化LED0的GPIO端口
    HAL_GPIO_Init(LED0_GPIO_PORT, &gpio_init_struct);

    // 配置LED1的GPIO初始化结构体
    gpio_init_struct.Pin = LED1_GPIO_PIN;
    // 初始化LED1的GPIO端口
    HAL_GPIO_Init(LED1_GPIO_PORT, &gpio_init_struct);

    // 默认状态下将LED0和LED1设置为高电平（熄灭）
    LED0(1);
    LED1(1);
}

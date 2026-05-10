#include "button.h"
#include "delay.h"

/**
 * @brief 初始化按键输入
 * 该函数用于初始化所有的按键输入，包括KEY0、KEY1、KEY2和WKUP。
 * 对于每个按键，函数会启用其GPIO时钟，配置GPIO端口为输入模式，并设置上拉或下拉电阻。
 *
 */
void button_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;
    /* 启用KEY0、KEY1、KEY2和WKUP的GPIO时钟 */
    KEY0_GPIO_CLK_ENABLE();
    KEY1_GPIO_CLK_ENABLE();
    KEY2_GPIO_CLK_ENABLE();
    WKUP_GPIO_CLK_ENABLE();

    /* 配置KEY0 GPIO端口 */
    gpio_init_struct.Pin = KEY0_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_INPUT;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(KEY0_GPIO_PORT, &gpio_init_struct);

    /* 配置KEY1 GPIO端口 */
    gpio_init_struct.Pin = KEY1_GPIO_PIN;
    HAL_GPIO_Init(KEY1_GPIO_PORT, &gpio_init_struct);

    /* 配置KEY2 GPIO端口 */
    gpio_init_struct.Pin = KEY2_GPIO_PIN;
    HAL_GPIO_Init(KEY2_GPIO_PORT, &gpio_init_struct);

    /* 配置WKUP GPIO端口 */
    gpio_init_struct.Pin = WKUP_GPIO_PIN;
    gpio_init_struct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(WKUP_GPIO_PORT, &gpio_init_struct);
}

/**
 * @brief 扫描并处理按键
 * 该函数用于扫描所有的按键状态，并根据模式参数处理按键的消抖和多重按键的问题。
 * 按照优先级顺序，按键的优先级从高到低为：WKUP、KEY2、KEY1、KEY0。
 *
 * @param mode 按键处理模式，0为单击模式，1为长按模式。
 * @return uint8_t 返回按下按键的标识符，如果没有按键按下，返回0。
 */
uint8_t button_scan(uint8_t mode)
{
    static uint8_t key_up = 1; /* 按键释放标志 */
    uint8_t keyval = 0;

    if (mode)
        key_up = 1; /* 长按模式下重置按键释放标志 */

    /* 当按键释放标志为1且有按键按下时进入处理流程 */
    if (key_up && (KEY0 == 0 || KEY1 == 0 || KEY2 == 0 || WK_UP == 1)) {
        delay_ms(10); /* 延迟以消抖 */
        key_up = 0;

        /* 依次判断哪个按键按下，并设置keyval的值 */
        if (KEY0 == 0)
            keyval = KEY0_PRES;
        else if (KEY1 == 0)
            keyval = KEY1_PRES;
        else if (KEY2 == 0)
            keyval = KEY2_PRES;
        else if (WK_UP == 1)
            keyval = WKUP_PRES;
    } else if (KEY0 == 1 && KEY1 == 1 && KEY2 == 1 && WK_UP == 0) /* 所有按键都释放时，重置按键释放标志 */
    {
        key_up = 1;
    }

    return keyval; /* 返回按键值 */
}


#include "iic.h"
#include "delay.h"

/**
 * @brief 初始化IIC
 * @param 无
 * @retval 无
 */
void iic_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    IIC_SCL_GPIO_CLK_ENABLE(); /* 启用SCL引脚的时钟 */
    IIC_SDA_GPIO_CLK_ENABLE(); /* 启用SDA引脚的时钟 */

    gpio_init_struct.Pin = IIC_SCL_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_OD;         /* 开漏输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                 /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;       /* 高速 */
    HAL_GPIO_Init(IIC_SCL_GPIO_PORT, &gpio_init_struct); /* 初始化SCL */

    gpio_init_struct.Pin = IIC_SDA_GPIO_PIN;
    HAL_GPIO_Init(IIC_SDA_GPIO_PORT, &gpio_init_struct); /* 初始化SDA */
    /* SDA设置为输出状态，开漏输出，上拉，高速，为发送数据做准备 */

    iic_stop(); /* 发送停止条件，结束通信 */
}

/**
 * @brief IIC延时函数，用于产生IIC通信中所需的微秒级延时
 * @param 无
 * @retval 无
 */
static void iic_delay(void)
{
    delay_us(2); /* 2us延时，用于实现250Khz的IIC通信频率 */
}

/**
 * @brief 发送IIC启动信号
 * @param 无
 * @retval 无
 */
void iic_start(void)
{
    IIC_SDA(1);
    IIC_SCL(1);
    iic_delay();
    IIC_SDA(0); /* 启动条件：SCL为高时，SDA由高到低的跳变，表示启动信号 */
    iic_delay();
    IIC_SCL(0); /* I2C总线空闲状态，等待设备响应 */
    iic_delay();
}

/**
 * @brief 发送IIC停止信号
 * @param 无
 * @retval 无
 */
void iic_stop(void)
{
    IIC_SDA(0); /* 停止条件：SCL为高时，SDA由低到高的跳变，表示停止信号 */
    iic_delay();
    IIC_SCL(1);
    iic_delay();
    IIC_SDA(1); /* 恢复I2C总线空闲状态 */
    iic_delay();
}

/**
 * @brief 等待从机发送ACK信号
 * @param 无
 * @retval 1表示未收到ACK信号，0表示成功收到ACK信号
 */
uint8_t iic_wait_ack(void)
{
    uint8_t waittime = 0;
    uint8_t rack = 0;

    IIC_SDA(1); /* 将SDA设置为高电平，准备接收ACK */
    iic_delay();
    IIC_SCL(1); /* SCL置高电平，等待从机ACK */
    iic_delay();

    while (IIC_READ_SDA) /* 等待ACK信号 */
    {
        waittime++;

        if (waittime > 250) {
            iic_stop();
            rack = 1;
            break;
        }
    }

    IIC_SCL(0); /* SCL置低电平，结束ACK信号检测 */
    iic_delay();
    return rack;
}

/**
 * @brief 发送ACK信号
 * @param 无
 * @retval 无
 */
void iic_ack(void)
{
    IIC_SDA(0); /* 在SCL上升沿时，SDA为低电平，表示发送ACK信号 */
    iic_delay();
    IIC_SCL(1); /* SCL保持高电平一个时钟周期 */
    iic_delay();
    IIC_SCL(0);
    iic_delay();
    IIC_SDA(1); /* SDA置高电平，结束ACK信号 */
    iic_delay();
}

/**
 * @brief 发送NAK信号
 * @param 无
 * @retval 无
 */
void iic_nack(void)
{
    IIC_SDA(1); /* 在SCL上升沿时，SDA为高电平，表示发送NAK信号 */
    iic_delay();
    IIC_SCL(1); /* SCL保持高电平一个时钟周期 */
    iic_delay();
    IIC_SCL(0);
    iic_delay();
}

/**
 * @brief IIC发送一个字节数据
 * @param data: 需要发送的数据
 * @retval 无
 */
void iic_send_byte(uint8_t data)
{
    uint8_t t;

    for (t = 0; t < 8; t++) {
        IIC_SDA((data & 0x80) >> 7); /* 按位发送数据 */
        iic_delay();
        IIC_SCL(1);
        iic_delay();
        IIC_SCL(0);
        data <<= 1; /* 数据左移，准备发送下一位 */
    }
    IIC_SDA(1); /* 数据发送完成后，SDA置高电平 */
}

/**
 * @brief IIC接收一个字节数据
 * @param ack: ack=1时发送ACK信号，ack=0时发送NAK信号
 * @retval 接收到的数据
 */
uint8_t iic_read_byte(uint8_t ack)
{
    uint8_t i, receive = 0;

    for (i = 0; i < 8; i++) /* 接收8位数据 */
    {
        receive <<= 1; /* 数据左移，为接收下一位数据做准备 */
        IIC_SCL(1);
        iic_delay();

        if (IIC_READ_SDA) {
            receive++; /* 接收到的位加到接收数据中 */
        }

        IIC_SCL(0);
        iic_delay();
    }

    if (!ack) {
        iic_nack(); /* 发送NAK信号 */
    } else {
        iic_ack(); /* 发送ACK信号 */
    }

    return receive;
}

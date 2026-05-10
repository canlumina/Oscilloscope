#ifndef __MYIIC_H
#define __MYIIC_H

#include "stm32f1xx.h"
// 定义IIC的硬件引脚
#define IIC_SCL_GPIO_PORT GPIOB
#define IIC_SCL_GPIO_PIN GPIO_PIN_10
#define IIC_SCL_GPIO_CLK_ENABLE()                                                                                                                                                  \
    do {                                                                                                                                                                           \
        __HAL_RCC_GPIOB_CLK_ENABLE();                                                                                                                                              \
    } while (0) /* PB口时钟使能 */

#define IIC_SDA_GPIO_PORT GPIOB
#define IIC_SDA_GPIO_PIN GPIO_PIN_11
#define IIC_SDA_GPIO_CLK_ENABLE()                                                                                                                                                  \
    do {                                                                                                                                                                           \
        __HAL_RCC_GPIOB_CLK_ENABLE();                                                                                                                                              \
    } while (0) /* PB口时钟使能 */

// IO操作宏定义
#define IIC_SCL(x)                                                                                                                                                                 \
    do {                                                                                                                                                                           \
        x ? HAL_GPIO_WritePin(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN, GPIO_PIN_SET) : HAL_GPIO_WritePin(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN, GPIO_PIN_RESET);                         \
    } while (0) /* SCL 高低电平控制 */

#define IIC_SDA(x)                                                                                                                                                                 \
    do {                                                                                                                                                                           \
        x ? HAL_GPIO_WritePin(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN, GPIO_PIN_SET) : HAL_GPIO_WritePin(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN, GPIO_PIN_RESET);                         \
    } while (0) /* SDA 高低电平控制 */

#define IIC_READ_SDA HAL_GPIO_ReadPin(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN) /* 读取SDA线电平 */

/* IIC软件模拟函数接口定义 */
void iic_init(void);                      /* 初始化IIC的GPIO口 */
void iic_start(void);                     /* 发送IIC开始信号 */
void iic_stop(void);                      /* 发送IIC停止信号 */
void iic_ack(void);                       /* 发送ACK信号 */
void iic_nack(void);                      /* 发送非ACK信号 */
uint8_t iic_wait_ack(void);               /* 等待从机发送ACK信号，返回值为ACK接收状态 */
void iic_send_byte(uint8_t txd);          /* 发送一个字节数据到IIC总线 */
uint8_t iic_read_byte(unsigned char ack); /* 从IIC总线读取一个字节数据，参数ack决定是否发送ACK */
#endif

#include "iic.h"
#include "24cxx.h"
#include "delay.h"

/**
 * @brief 初始化AT24CXX IIC通信
 * 该函数用于初始化IIC通信，为AT24CXX的后续操作做准备。
 * @param 无
 * @retval 无
 */
void at24cxx_init(void)
{
    iic_init(); // 初始化IIC通信
}

/**
 * @brief 从AT24CXX中读取一个字节数据
 * 该函数通过IIC通信从指定地址读取一个字节的数据。
 * @param readaddr: 要读取数据的起始地址
 * @retval 读取到的数据
 */
uint8_t at24cxx_read_one_byte(uint16_t addr)
{
    uint8_t temp = 0;
    iic_start(); /* 发起IIC开始信号 */

    /* 根据AT24CXX类型选择地址模式，发送高8位地址 */
    if (EE_TYPE > AT24C16) /* 适用于24C16及以上容量的芯片，需发送2个地址字节 */
    {
        iic_send_byte(0XA0);      /* 发送写指令地址，IIC地址的第1个字节 */
        iic_wait_ack();           /* 等待ACK确认 */
        iic_send_byte(addr >> 8); /* 发送地址的高8位 */
    }
    else
    {
        iic_send_byte(0XA0 + ((addr >> 8) << 1)); /* 直接发送包含类型信息的地址 */
    }

    iic_wait_ack();            /* 等待ACK确认 */
    iic_send_byte(addr % 256); /* 发送地址的低8位 */
    iic_wait_ack();            /* 等待ACK确认 */

    iic_start();             /* 发起IIC重新开始信号 */
    iic_send_byte(0XA1);     /* 发送读指令地址，IIC地址的第1个字节 */
    iic_wait_ack();          /* 等待ACK确认 */
    temp = iic_read_byte(0); /* 读取一个字节的数据 */
    iic_stop();              /* 发起IIC停止信号 */
    return temp;
}

/**
 * @brief 向AT24CXX中写入一个字节数据
 * 该函数通过IIC通信将一个字节的数据写入到指定地址。
 * @param addr: 写入数据的起始地址
 * @param data: 要写入的数据
 * @retval 无
 */
void at24cxx_write_one_byte(uint16_t addr, uint8_t data)
{
    /* 详细流程参见at24cxx_read_one_byte函数注释，主要区别在于最后发送的是数据而不是地址 */
    iic_start(); /* 发起IIC开始信号 */
    if (EE_TYPE > AT24C16)
    {
        iic_send_byte(0XA0); /* 发送写指令地址 */
        iic_wait_ack();
        iic_send_byte(addr >> 8); /* 发送地址的高8位 */
    }
    else
    {
        iic_send_byte(0XA0 + ((addr >> 8) << 1));
    }

    iic_wait_ack();
    iic_send_byte(addr % 256); /* 发送地址的低8位 */
    iic_wait_ack();
    iic_send_byte(data); /* 发送数据 */
    iic_wait_ack();
    iic_stop();   /* 发起IIC停止信号 */
    delay_ms(10); /* 等待写入完成 */
}

/**
 * @brief 检测AT24CXX是否存在
 * 通过读取和写入特定地址的数据来检测AT24CXX是否存在和工作正常。
 * @param 无
 * @retval uint8_t: 检测结果，0表示正常，1表示异常
 */
uint8_t at24cxx_check(void)
{
    uint8_t temp;
    uint16_t addr = EE_TYPE;
    temp = at24cxx_read_one_byte(addr); /* 读取预设地址的数据 */

    if (temp == 0X55) /* 检查读取的数据是否符合预期 */
    {
        return 0;
    }
    else /* 如果数据不符合预期，则尝试写入预期数据并重新读取 */
    {
        at24cxx_write_one_byte(addr, 0X55); /* 写入预期数据 */
        temp = at24cxx_read_one_byte(255);  /* 重新读取任意地址的数据 */

        if (temp == 0X55)
            return 0; /* 检查重新读取的数据是否符合预期 */
    }

    return 1; /* 如果数据仍然不符合预期，则返回检测异常 */
}

/**
 * @brief 从AT24CXX中读取指定长度的数据
 * 该函数用于从AT24CXX的指定地址开始读取一定长度的数据到缓冲区。
 * @param addr: 读取数据的起始地址
 * @param pbuf: 用于存储读取数据的缓冲区指针
 * @param datalen: 需要读取的数据长度
 * @retval 无
 */
void at24cxx_read(uint16_t addr, uint8_t *pbuf, uint16_t datalen)
{
    /* 循环读取数据到缓冲区 */
    while (datalen--)
    {
        *pbuf++ = at24cxx_read_one_byte(addr++); /* 读取一个字节数据并存入缓冲区 */
    }
}

/**
 * @brief 向AT24CXX中写入指定长度的数据
 * 该函数用于将指定长度的数据从缓冲区写入到AT24CXX的指定地址。
 * @param addr: 写入数据的起始地址
 * @param pbuf: 包含需要写入数据的缓冲区指针
 * @param datalen: 需要写入的数据长度
 * @retval 无
 */
void at24cxx_write(uint16_t addr, uint8_t *pbuf, uint16_t datalen)
{
    /* 循环将数据从缓冲区写入AT24CXX */
    while (datalen--)
    {
        at24cxx_write_one_byte(addr, *pbuf); /* 写入一个字节数据 */
        addr++;                              /* 地址自增 */
        pbuf++;                              /* 缓冲区指针自增 */
    }
}

void at24cxx_write_len_byte(uint16_t WriteAddr, uint32_t DataToWrite, uint8_t Len)
{
    /* 循环将一个32位数据按字节写入AT24CXX */
    uint8_t t;
    for (t = 0; t < Len; t++)
    {
        at24cxx_write_one_byte(WriteAddr + t, (DataToWrite >> (8 * t)) & 0xff); /* 按位移位后写入 */
    }
}

uint32_t at24cxx_read_len_byte(uint16_t ReadAddr, uint8_t Len)
{
    /* 循环从AT24CXX读取Len个字节并合成一个32位数据 */
    uint8_t t;
    uint32_t temp = 0;
    for (t = 0; t < Len; t++)
    {
        temp <<= 8;                                            /* 左移8位准备接收下一个字节 */
        temp += at24cxx_read_one_byte(ReadAddr + Len - t - 1); /* 读取一个字节并累加 */
    }
    return temp;
}

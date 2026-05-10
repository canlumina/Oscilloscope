#ifndef __24CXX_H
#define __24CXX_H

#include "stm32f1xx.h"

// 定义不同容量的24CXX EEPROM型号对应的地址空间大小
#define AT24C01 127
#define AT24C02 255
#define AT24C04 511
#define AT24C08 1023
#define AT24C16 2047
#define AT24C32 4095
#define AT24C64 8191
#define AT24C128 16383
#define AT24C256 32767

// 默认使用的是24C02型号的EEPROM
#define EE_TYPE AT24C02

/**
 * 初始化IIC通信，为访问24CXX EEPROM做好准备。
 */
void at24cxx_init(void);

/**
 * 检查EEPROM是否存在和可访问。
 * @return 返回值为1表示成功，其他值表示失败。
 */
uint8_t at24cxx_check(void);

/**
 * 从指定地址读取一个字节数据。
 * @param addr 读取数据的起始地址。
 * @return 读取到的数据。
 */
uint8_t at24cxx_read_one_byte(uint16_t addr);

/**
 * 向指定地址写入一个字节数据。
 * @param addr 写入数据的起始地址。
 * @param data 要写入的数据。
 */
void at24cxx_write_one_byte(uint16_t addr, uint8_t data);

/**
 * 向EEPROM写入数据。
 * @param addr 写入数据的起始地址。
 * @param pbuf 数据缓冲区指针。
 * @param datalen 要写入的数据长度。
 */
void at24cxx_write(uint16_t addr, uint8_t *pbuf, uint16_t datalen);

/**
 * 从EEPROM读取数据。
 * @param addr 读取数据的起始地址。
 * @param pbuf 用于存储读取数据的缓冲区指针。
 * @param datalen 要读取的数据长度。
 */
void at24cxx_read(uint16_t addr, uint8_t *pbuf, uint16_t datalen);

/**
 * 向指定地址写入指定长度的数据。
 * @param WriteAddr 写入数据的起始地址。
 * @param DataToWrite 要写入的数据。
 * @param Len 要写入的数据长度。
 */
void at24cxx_write_len_byte(uint16_t WriteAddr, uint32_t DataToWrite, uint8_t Len);

/**
 * 从指定地址读取指定长度的数据。
 * @param ReadAddr 读取数据的起始地址。
 * @param Len 要读取的数据长度。
 * @return 读取到的数据。
 */
uint32_t at24cxx_read_len_byte(uint16_t ReadAddr, uint8_t Len);
#endif

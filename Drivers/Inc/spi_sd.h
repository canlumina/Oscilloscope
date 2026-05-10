#ifndef __BSP_SDCARD_H__
#define __BSP_SDCARD_H__

#include "spi.h"

/**
 * 定义SD卡选择信号的GPIO端口、引脚及使能GPIO时钟的宏。
 *
 * 本宏定义了用于控制SD卡选择信号的GPIO端口、引脚，并提供了一个宏函数来使能该GPIO端口的时钟。
 *这对于操作SD卡是必需的。
 */

// 定义SD卡CS信号所使用的GPIO端口
#define SD_CS_GPIO_PORT GPIOD
// 定义SD卡CS信号所使用的GPIO引脚
#define SD_CS_GPIO_PIN GPIO_PIN_2
// 定义使能SD卡CS信号所使用GPIO端口的时钟的宏函数
#define SD_CS_GPIO_CLK_ENABLE()                                                                                                                                                    \
    do {                                                                                                                                                                           \
        __HAL_RCC_GPIOD_CLK_ENABLE();                                                                                                                                              \
    } while (0)

/**
 * 通过SPI读写一个字节数据
 * @param x 要读写的数据字节
 * @return 无返回值
 */
#define sd_spi_read_write_byte(x) spi2_read_write_byte(x)

/**
 * 设置SPI速度为低速
 * @param 无参数
 * @return 无返回值
 */
#define sd_spi_speed_low() spi2_set_speed(SPI_SPEED_256)

/**
 * 设置SPI速度为高速
 * @param 无参数
 * @return 无返回值
 */
#define sd_spi_speed_high() spi2_set_speed(SPI_SPEED_2)

/**
 * 控制SD卡的选通信号
 *
 * 该宏通过一个条件表达式来控制SD卡的选通信号（CS）的高低电平。
 * 当条件x为真时，设置CS信号为高电平；当条件x为假时，设置CS信号为低电平。
 *
 * @param x 条件表达式，用于决定SD卡CS信号的状态。
 */
#define SD_CS(x)                                                                                                                                                                   \
    do {                                                                                                                                                                           \
        x ? HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_GPIO_PIN, GPIO_PIN_SET) : HAL_GPIO_WritePin(SD_CS_GPIO_PORT, SD_CS_GPIO_PIN, GPIO_PIN_RESET);                                 \
    } while (0) /* SD_CS */

// 定义SD卡操作返回状态
#define SD_OK 0    // 操作成功
#define SD_ERROR 1 // 操作错误

// 定义SD卡类型
#define SD_TYPE_ERR 0X00  // 类型错误
#define SD_TYPE_MMC 0X01  // MMC卡
#define SD_TYPE_V1 0X02   // SD卡v1.0
#define SD_TYPE_V2 0X04   // SD卡v2.0
#define SD_TYPE_V2HC 0X06 // SD卡v2.0高容量

// 定义SD卡命令
#define CMD0 (0)           /* GO_IDLE_STATE */
#define CMD1 (1)           /* SEND_OP_COND (MMC) */
#define ACMD41 (0x80 + 41) /* SEND_OP_COND (SDC) */
#define CMD8 (8)           /* SEND_IF_COND */
#define CMD9 (9)           /* SEND_CSD */
#define CMD10 (10)         /* SEND_CID */
#define CMD12 (12)         /* STOP_TRANSMISSION */
#define ACMD13 (0x80 + 13) /* SD_STATUS (SDC) */
#define CMD16 (16)         /* SET_BLOCKLEN */
#define CMD17 (17)         /* READ_SINGLE_BLOCK */
#define CMD18 (18)         /* READ_MULTIPLE_BLOCK */
#define CMD23 (23)         /* SET_BLOCK_COUNT (MMC) */
#define ACMD23 (0x80 + 23) /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24 (24)         /* WRITE_BLOCK */
#define CMD25 (25)         /* WRITE_MULTIPLE_BLOCK */
#define CMD32 (32)         /* ERASE_ER_BLK_START */
#define CMD33 (33)         /* ERASE_ER_BLK_END */
#define CMD38 (38)         /* ERASE */
#define CMD55 (55)         /* APP_CMD */
#define CMD58 (58)         /* READ_OCR */

extern uint8_t sd_type;

// 定义SD卡类型的全局变量
extern uint8_t sd_type;

// SPI接口初始化
static void sd_spi_init(void);

// 使SD卡去选
static void sd_deselect(void);

// 选中SD卡
// 返回值：操作成功返回0，失败返回非0值
static uint8_t sd_select(void);

// 等待SD卡准备好
// 返回值：操作成功返回0，失败返回非0值
static uint8_t sd_wait_ready(void);

// 获取SD卡的响应
// 参数：response - 需要获取的响应类型
// 返回值：成功返回响应数据，失败返回错误码
static uint8_t sd_get_response(uint8_t response);

// 发送命令给SD卡
// 参数：cmd - 待发送的命令码, arg - 命令参数
// 返回值：成功返回0，失败返回非0值
static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg);

// 发送数据块到SD卡
// 参数：buf - 待发送数据的缓冲区, cmd - 伴随数据发送的命令码
// 返回值：成功返回0，失败返回非0值
static uint8_t sd_send_block(uint8_t *buf, uint8_t cmd);

// 从SD卡接收数据块
// 参数：buf - 用于接收数据的缓冲区, len - 需要接收的数据长度
// 返回值：成功返回0，失败返回非0值
static uint8_t sd_receive_data(uint8_t *buf, uint16_t len);

// 初始化SD卡
// 返回值：成功返回0，失败返回非0值
uint8_t sd_init(void);

// 获取SD卡的扇区数量
// 返回值：SD卡的总扇区数
uint32_t sd_get_sector_count(void);

// 获取SD卡的状态
// 返回值：成功返回0，失败返回非0值
uint8_t sd_get_status(void);

// 获取SD卡的CID（卡标识符）
// 参数：cid_data - 用于存储CID数据的缓冲区
// 返回值：成功返回0，失败返回非0值
uint8_t sd_get_cid(uint8_t *cid_data);

// 获取SD卡的CSD（卡特性描述符）
// 参数：csd_data - 用于存储CSD数据的缓冲区
// 返回值：成功返回0，失败返回非0值
uint8_t sd_get_csd(uint8_t *csd_data);

// 从SD卡读取数据
// 参数：pbuf - 用于存储读取数据的缓冲区, saddr - 起始地址, cnt - 要读取的扇区数量
// 返回值：成功返回0，失败返回非0值
uint8_t sd_read_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt);

// 向SD卡写入数据
// 参数：pbuf - 待写入数据的缓冲区, saddr - 起始地址, cnt - 要写入的扇区数量
// 返回值：成功返回0，失败返回非0值
uint8_t sd_write_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt);

#endif // !__BSP_SDCARD_H__

#include "spi_sd.h"

uint8_t sd_type = 0;

/**
 * 初始化SPI和SD卡的CS引脚
 * 该函数初始化SPI接口以及SD卡的CS控制引脚，为后续的SPI通信与SD卡操作做准备。
 * 该函数不接受参数，也不返回任何值。
 */
static void sd_spi_init(void)
{
    GPIO_InitTypeDef gpio_init_struct;

    // 启用SD卡CS引脚的时钟
    SD_CS_GPIO_CLK_ENABLE();
    // 配置SD卡CS引脚
    gpio_init_struct.Pin = SD_CS_GPIO_PIN;
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init_struct.Pull = GPIO_PULLUP;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    // 初始化GPIO端口
    HAL_GPIO_Init(SD_CS_GPIO_PORT, &gpio_init_struct);

    // 初始化SPI2
    spi2_init();
    // 将SD卡CS引脚置高电平
    SD_CS(1);
}

/**
 * @brief       SD卡取消选择，释放SPI总线
 * @param       无
 * @retval      无
 */
static void sd_deselect(void)
{
    SD_CS(1);                     /* 取消SD卡片选信号 */
    sd_spi_read_write_byte(0xff); /* 提供额外的8个时钟周期 */
}

/**
 * @brief       SD卡选择，并等待卡准备好
 * @param       无
 * @retval      选择结果
 *              SD_OK,      选择成功
 *              SD_ERROR,   选择失败
 */
static uint8_t sd_select(void)
{
    SD_CS(0); /* 使能SD卡片选信号 */

    if (sd_wait_ready() == 0) /* 等待SD卡准备好 */
    {
        return SD_OK; /* 等待成功 */
    }

    sd_deselect();   /* 选择失败，取消SD卡选择 */
    return SD_ERROR; /* 返回选择失败 */
}

/**
 * @brief   等待SD卡准备就绪
 * @param   无
 * @retval  等待结果
 *          SD_OK,      成功
 *          SD_ERROR,   失败
 */
static uint8_t sd_wait_ready(void)
{
    uint32_t t = 0;

    do {
        // 尝试读取SD卡状态，如果返回0xFF，则表示卡已就绪
        if (sd_spi_read_write_byte(0XFF) == 0XFF) {
            return SD_OK; /* 成功 */
        }

        t++;
    } while (t < 0XFFFFFF); /* 循环等待，直到超时 */

    return SD_ERROR; /* 超时未就绪，返回错误 */
}
/**
 * @brief       等待SD卡响应
 * @param       response : 期待收到的响应值
 * @retval      等待结果
 *              SD_OK,      成功
 *              SD_ERROR,   失败
 */
static uint8_t sd_get_response(uint8_t response)
{
    uint16_t count = 0xFFFF; /* 等待次数计数器初始化 */

    /* 循环等待直到收到预期的响应值或等待次数用尽 */
    while ((sd_spi_read_write_byte(0XFF) != response) && count) {
        count--; /* 每次循环递减等待次数 */
    }

    /* 判断是否等待超时 */
    if (count == 0) {
        return SD_ERROR; /* 等待超时，返回错误 */
    }

    return SD_OK; /* 收到正确响应，返回成功 */
}
/**
 * 从SD卡读取一次数据
 * @brief 从SD卡读取指定长度的数据到缓冲区
 * @note 读取长度不限制，由参数len指定，通常为512字节
 * @param buf 数据存储的缓冲区
 * @param len 要读取的数据长度
 * @retval 读取操作的状态
 *          - SD_OK: 读取成功
 *          - SD_ERROR: 读取失败
 */
static uint8_t sd_receive_data(uint8_t *buf, uint16_t len)
{
    // 等待SD卡返回数据开始标志0xFE
    if (sd_get_response(0xFE)) {
        return SD_ERROR;
    }

    // 循环接收数据，直到接收完指定长度
    while (len--) {
        *buf = sd_spi_read_write_byte(0xFF);
        buf++;
    }

    // 接收两个假CRC字节（dummy CRC）
    sd_spi_read_write_byte(0xFF);
    sd_spi_read_write_byte(0xFF);

    return SD_OK; // 读取成功
}

/**
 * @brief 向SD卡写入一个数据块
 * @note 写入长度固定为512字节！
 * @param buf 数据缓存区
 * @param cmd 命令码
 * @retval 发送结果
 *          SD_OK, 成功
 *          SD_ERROR, 失败
 */
static uint8_t sd_send_block(uint8_t *buf, uint8_t cmd)
{
    uint16_t t;

    if (sd_wait_ready()) /* 等待SD卡就绪 */
    {
        return SD_ERROR;
    }

    sd_spi_read_write_byte(cmd); /* 发送命令 */

    if (cmd != 0XFD) /* 如果不是结束命令 */
    {
        /* 发送512字节的数据 */
        for (t = 0; t < 512; t++) {
            sd_spi_read_write_byte(buf[t]);
        }

        /* 忽略CRC */
        sd_spi_read_write_byte(0xFF);
        sd_spi_read_write_byte(0xFF);

        /* 接收响应 */
        t = sd_spi_read_write_byte(0xFF);

        if ((t & 0x1F) != 0x05) /* 检查数据包是否被接收 */
        {
            return SD_ERROR;
        }
    }

    return SD_OK; /* 写入成功 */
}

/**
 * @brief 向SD卡发送一个命令
 * @note 不同命令的CRC值在该函数内部自动确认
 * @param cmd 要发送的命令
 * @note 最高位为1表示ACMD（应用命令），最高位为0表示CMD（普通命令）
 * @param arg 命令的参数
 * @retval SD卡返回的命令响应
 */
static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg)
{
    uint8_t res;
    uint8_t retry = 0;
    uint8_t crc = 0X01; /* 默认 CRC = 忽略CRC + 停止位 */

    if (cmd & 0x80) /* 如果是ACMD，在发送前需要先发送一个CMD55 */
    {
        cmd &= 0x7F;                 /* 清除最高位，获取ACMD命令 */
        res = sd_send_cmd(CMD55, 0); /* 发送CMD55 */

        if (res > 1) {
            return res;
        }
    }

    if (cmd != CMD12) /* 当 cmd 不等于 多块读取结束命令(CMD12)时, 等待卡选择成功 */
    {
        sd_deselect(); /* 取消上一次的片选 */

        if (sd_select()) {
            return 0xFF; /* 选择失败返回 */
        }
    }

    /* 发送命令包 */
    sd_spi_read_write_byte(cmd | 0x40); /* 起始位 + 命令索引号 */
    sd_spi_read_write_byte(arg >> 24);  /* 参数[31 : 24] */
    sd_spi_read_write_byte(arg >> 16);  /* 参数[23 : 16] */
    sd_spi_read_write_byte(arg >> 8);   /* 参数[15 : 8] */
    sd_spi_read_write_byte(arg);        /* 参数[7 : 0] */

    /* 根据不同命令设置CRC值 */
    if (cmd == CMD0)
        crc = 0X95; /* CMD0 的CRC固定为 0X95 */
    if (cmd == CMD8)
        crc = 0X87; /* CMD8 的CRC固定为 0X87 */

    sd_spi_read_write_byte(crc);

    if (cmd == CMD12) /* 如果命令是CMD12（多块读取结束命令） */
    {
        sd_spi_read_write_byte(0xFF); /* CMD12需要跳过一个字节 */
    }

    retry = 10; /* 设置重试次数 */

    do /* 等待响应或超时退出 */
    {
        res = sd_spi_read_write_byte(0xFF);
    } while ((res & 0X80) && retry--);

    return res; /* 返回状态码 */
}
/**
 * @brief       获取SD卡的状态
 * @param       无
 * @retval      读取结果
 *              SD_OK,      成功
 *              SD_ERROR,   失败
 */
uint8_t sd_get_status(void)
{
    uint8_t res;
    uint8_t retry = 20; /* 发送ACMD经常失败，多次尝试 */

    do {
        res = sd_send_cmd(ACMD13, 0); /* 发送ACMD13命令，获取状态 */
    } while (res && retry--);

    sd_deselect(); /* 取消卡选择 */

    return res;
}

/**
 * @brief       获取SD卡的CID信息，包括制造商信息等
 * @param       cid_data : 存储CID的内存缓冲区(至少16字节)
 * @retval      读取结果
 *              SD_OK,      成功
 *              SD_ERROR,   失败
 */
uint8_t sd_get_cid(uint8_t *cid_data)
{
    uint8_t res;

    res = sd_send_cmd(CMD10, 0); /* 发送CMD10命令，读CID */

    if (res == 0x00) {
        res = sd_receive_data(cid_data, 16); /* 接收16字节的数据 */
    }

    sd_deselect(); /* 取消卡选择 */

    return res;
}

/**
 * @brief       从SD卡获取CSD（Card Specific Data）信息，包括容量和速度信息
 * @param       csd_data : 用于存储CSD信息的内存缓冲区(至少16字节)
 * @retval      返回读取操作的结果状态
 *              SD_OK,      读取成功
 *              SD_ERROR,   读取失败
 */
uint8_t sd_get_csd(uint8_t *csd_data)
{
    uint8_t res;

    // 发送CMD9命令，读取CSD信息
    res = sd_send_cmd(CMD9, 0);

    if (res == 0) {
        // 接收16字节的CSD数据
        res = sd_receive_data(csd_data, 16);
    }

    // 取消卡的选择状态
    sd_deselect();

    return res;
}

/**
 * @brief 获取SD卡的总扇区数（扇区数）
 * @note 每个扇区的字节数必须为512，如果不是512，则初始化无法通过
 * @param 无
 * @retval SD卡容量（扇区数），转换成字节数要乘以512
 */
uint32_t sd_get_sector_count(void)
{
    uint8_t csd[16];   // 用于存储CSD（卡特定数据）的数组
    uint32_t capacity; // 存储卡容量
    uint8_t n;         // 临时变量，用于计算
    uint16_t csize;    // 存储卡的逻辑区块大小

    if (sd_get_csd(csd) != 0) // 获取CSD信息，如果失败则返回0
    {
        return 0; // 返回0表示获取容量失败
    }

    /* 如果是SDHC卡，按照以下方式计算 */
    if ((csd[0] & 0xC0) == 0x40) // 判断是否为V2.00版本的卡
    {
        // 计算逻辑扇区数
        csize = csd[9] + ((uint16_t)csd[8] << 8) + ((uint32_t)(csd[7] & 63) << 16) + 1;
        capacity = (uint32_t)csize << 10; // 得到扇区数
    } else                                // V1.XX版本的SD卡或MMC V3卡
    {
        // 计算版本1的卡或MMC v3的卡的扇区数
        n = (csd[5] & 15) + ((csd[10] & 128) >> 7) + ((csd[9] & 3) << 1) + 2;
        csize = (csd[8] >> 6) + ((uint16_t)csd[7] << 2) + ((uint16_t)(csd[6] & 3) << 10) + 1;
        capacity = (uint32_t)csize << (n - 9); // 得到扇区数
    }

    return capacity; // 返回扇区数，注意这里是扇区数，不是实际容量的字节数，要换算成字节需乘以512
}

/**
 * 初始化SD卡
 * 该函数用于初始化SD卡，使其进入可用状态。
 *
 * @param  无参数。
 * @retval 返回初始化结果，使用SD_OK表示成功，SD_ERROR表示失败。
 */
uint8_t sd_init(void)
{
    uint8_t res;    /* 用于存储SD卡命令响应的结果 */
    uint16_t retry; /* 用于超时重试计数 */
    uint8_t ocr[4]; /* 用于存储操作条件响应（OCR）的缓存 */
    uint16_t i;
    uint8_t cmd;

    /* 初始化SPI通信，将通信速度设置为低速 */
    sd_spi_init();
    sd_spi_speed_low();

    /* 发送至少74个时钟周期的0xFF，进行时钟同步 */
    for (i = 0; i < 10; i++) {
        sd_spi_read_write_byte(0XFF);
    }

    /* 尝试发送CMD0，让SD卡进入空闲状态 */
    retry = 20;
    do {
        res = sd_send_cmd(CMD0, 0);
    } while ((res != 0X01) && retry--);

    sd_type = 0; /* 初始化时默认无卡 */

    /* 如果CMD0发送成功 */
    if (res == 0X01) {
        /* 检测SD卡版本是否为2.0 */
        if (sd_send_cmd(CMD8, 0x1AA) == 1) {
            /* 读取R7响应的后4个字节 */
            for (i = 0; i < 4; i++) {
                ocr[i] = sd_spi_read_write_byte(0XFF);
            }

            /* 检查OCR是否支持2.7~3.6V的工作电压 */
            if (ocr[2] == 0X01 && ocr[3] == 0XAA) {
                /* 尝试发送ACMD41，进入就绪状态 */
                retry = 1000;
                do {
                    res = sd_send_cmd(ACMD41, 1UL << 30);
                } while (res && retry--);

                /* 检测是否为SD2.0卡 */
                if (retry && sd_send_cmd(CMD58, 0) == 0) {
                    for (i = 0; i < 4; i++) {
                        ocr[i] = sd_spi_read_write_byte(0XFF); /* 读取OCR值 */
                    }

                    /* 根据OCR判断是SDHC卡还是SD卡 */
                    if (ocr[0] & 0x40) {
                        sd_type = SD_TYPE_V2HC; /* SDHC卡 */
                    } else {
                        sd_type = SD_TYPE_V2; /* SD卡 */
                    }
                }
            }
        } else /* 如果不是SD 2.0卡，则判断为SD 1.0卡或MMC卡 */
        {
            res = sd_send_cmd(ACMD41, 0);
            retry = 1000;

            /* 根据响应结果判断是SD 1.0卡还是MMC卡 */
            if (res <= 1) {
                sd_type = SD_TYPE_V1; /* SD 1.0卡 */
                cmd = ACMD41;         /* 使用ACMD41命令 */
            } else {
                sd_type = SD_TYPE_MMC; /* MMC卡 */
                cmd = CMD1;            /* 使用CMD1命令 */
            }

            /* 尝试发送命令，使卡退出空闲状态 */
            do {
                res = sd_send_cmd(cmd, 0);
            } while (res && retry--);

            /* 如果退出空闲状态失败或不支持512字节块长度，则认为卡错误 */
            if (retry == 0 || sd_send_cmd(CMD16, 512) != 0) {
                sd_type = SD_TYPE_ERR; /* 错误的卡 */
            }
        }
    }

    /* 取消片选，恢复SPI高速模式 */
    sd_deselect();
    sd_spi_speed_high();

    /* 如果成功识别卡类型，则返回初始化成功 */
    if (sd_type) {
        return SD_OK;
    }

    /* 如果初始化失败，则返回错误码 */
    return res;
}

/**
 * @brief 读取SD卡（fatfs/usb调用）
 *
 * @param pbuf 数据缓存区
 * @param saddr 起始扇区地址
 * @param cnt 要读取的扇区数量
 * @retval 读取结果
 *          - SD_OK, 成功
 *          - SD_ERROR, 失败
 */
uint8_t sd_read_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt)
{
    uint8_t res;
    long long lsaddr = saddr;

    // 根据SD卡类型，转换起始地址为字节地址
    if (sd_type != SD_TYPE_V2HC) {
        lsaddr <<= 9; /* 转换为字节地址 */
    }

    // 单个扇区读取逻辑
    if (cnt == 1) {
        res = sd_send_cmd(CMD17, lsaddr); /* 发送读扇区命令 */

        if (res == 0) /* 命令发送成功 */
        {
            res = sd_receive_data(pbuf, 512); /* 接收512个字节数据 */
        }
    } else // 多扇区连续读取逻辑
    {
        res = sd_send_cmd(CMD18, lsaddr); /* 发送连续读扇区命令 */

        do {
            res = sd_receive_data(pbuf, 512); /* 接收512个字节数据 */
            pbuf += 512;                      /* 更新数据缓存区指针 */
        } while (--cnt && res == 0); /* 循环直到读取完所有扇区或发生错误 */

        sd_send_cmd(CMD12, 0); /* 发送停止传输命令 */
    }

    sd_deselect(); /* 取消卡的选择 */
    return res;    /* 返回操作结果 */
}

/**
 * @brief       写入SD卡（fatfs/usb调用）
 * @param       pbuf: 数据缓存区
 * @param       saddr: 扇区地址
 * @param       cnt: 扇区数量
 * @retval      写入结果
 *              SD_OK,      成功
 *              SD_ERROR,   失败
 */
uint8_t sd_write_disk(uint8_t *pbuf, uint32_t saddr, uint32_t cnt)
{
    uint8_t retry = 20; /* 发送ACMD经常失败，多次尝试 */
    uint8_t res;
    long long lsaddr = saddr;

    /* 转换地址格式为字节地址 */
    if (sd_type != SD_TYPE_V2HC) {
        lsaddr <<= 9;
    }

    /* 单个扇区写入逻辑 */
    if (cnt == 1) {
        res = sd_send_cmd(CMD24, lsaddr); /* 发送写扇区命令 */

        if (res == 0) /* 命令发送成功 */
        {
            res = sd_send_block(pbuf, 0xFE); /* 写入512个字节 */
        }
    } else /* 多扇区写入逻辑 */
    {
        /* 非MMC类型卡发送ACMD23命令设置写入扇区数 */
        if (sd_type != SD_TYPE_MMC) {
            do {
                sd_send_cmd(ACMD23, cnt);
            } while (res && retry--);
        }

        res = sd_send_cmd(CMD25, lsaddr); /* 发送连续写入命令 */

        if (res == 0) {
            /* 循环写入每个扇区，每次写入512个字节 */
            do {
                res = sd_send_block(pbuf, 0xFC);
                pbuf += 512;
            } while (--cnt && res == 0);

            /* 发送写入结束块命令 */
            res = sd_send_block(0, 0xFD);
        }
    }

    sd_deselect(); /* 取消卡选择 */
    return res;
}

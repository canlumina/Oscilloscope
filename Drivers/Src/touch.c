#include "touch.h"
#include "lcd.h"
#include "delay.h"
#include "stdlib.h"
#include <math.h>
#include "24cxx.h"
//////////////////////////////////////////////////////////////////////////////////
//****************************************
// V2.0修改说明
// 增加对电容触摸屏的支持(需要添加:ctiic.c和ott2001a.c两个文件)
//////////////////////////////////////////////////////////////////////////////////

_m_tp_dev tp_dev = {
    TP_Init,
    TP_Scan,
    tp_adjust,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};
// 默认为touchtype=0的数据.
uint8_t CMD_RDX = 0XD0;
uint8_t CMD_RDY = 0X90;

// SPI写数据
// 向触摸屏IC写入1byte数据
// num:要写入的数据
void TP_Write_Byte(uint8_t num)
{
    uint8_t count = 0;
    for (count = 0; count < 8; count++)
    {
        if (num & 0x80)
            TDIN = 1;
        else
            TDIN = 0;
        num <<= 1;
        TCLK = 0;
        TCLK = 1; // 上升沿有效
    }
}
// SPI读数据
// 从触摸屏IC读取adc值
// CMD:指令
// 返回值:读到的数据
uint16_t TP_Read_AD(uint8_t CMD)
{
    uint8_t count = 0;
    uint16_t Num = 0;
    TCLK = 0;           // 先拉低时钟
    TDIN = 0;           // 拉低数据线
    TCS = 0;            // 选中触摸屏IC
    TP_Write_Byte(CMD); // 发送命令字
    delay_us(6);        // ADS7846的转换时间最长为6us
    TCLK = 0;
    delay_us(1);
    TCLK = 1; // 给1个时钟，清除BUSY
    TCLK = 0;
    for (count = 0; count < 16; count++) // 读出16位数据,只有高12位有效
    {
        Num <<= 1;
        TCLK = 0; // 下降沿有效
        TCLK = 1;
        if (DOUT)
            Num++;
    }
    Num >>= 4; // 只有高12位有效.
    TCS = 1;   // 释放片选
    return (Num);
}
// 读取一个坐标值(x或者y)
// 连续读取READ_TIMES次数据,对这些数据升序排列,
// 然后去掉最低和最高LOST_VAL个数,取平均值
// xy:指令（CMD_RDX/CMD_RDY）
// 返回值:读到的数据
#define READ_TIMES 5 // 读取次数
#define LOST_VAL 1   // 丢弃值
uint16_t TP_Read_XOY(uint8_t xy)
{
    uint16_t i, j;
    uint16_t buf[READ_TIMES];
    uint16_t sum = 0;
    uint16_t temp;
    for (i = 0; i < READ_TIMES; i++)
        buf[i] = TP_Read_AD(xy);
    for (i = 0; i < READ_TIMES - 1; i++) // 排序
    {
        for (j = i + 1; j < READ_TIMES; j++)
        {
            if (buf[i] > buf[j]) // 升序排列
            {
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }
    sum = 0;
    for (i = LOST_VAL; i < READ_TIMES - LOST_VAL; i++)
        sum += buf[i];
    temp = sum / (READ_TIMES - 2 * LOST_VAL);
    return temp;
}
// 读取x,y坐标
// 最小值不能少于100.
// x,y:读取到的坐标值
// 返回值:0,失败;1,成功。
uint8_t TP_Read_XY(uint16_t *x, uint16_t *y)
{
    uint16_t xtemp, ytemp;
    xtemp = TP_Read_XOY(CMD_RDX);
    ytemp = TP_Read_XOY(CMD_RDY);
    // if(xtemp<100||ytemp<100)return 0;//读数失败
    *x = xtemp;
    *y = ytemp;
    return 1; // 读数成功
}
// 连续2次读取触摸屏IC,且这两次的偏差不能超过
// ERR_RANGE,满足条件,则认为读数正确,否则读数错误.
// 该函数能大大提高准确度
// x,y:读取到的坐标值
// 返回值:0,失败;1,成功。
#define ERR_RANGE 50 // 误差范围
uint8_t TP_Read_XY2(uint16_t *x, uint16_t *y)
{
    uint16_t x1, y1;
    uint16_t x2, y2;
    uint8_t flag;
    flag = TP_Read_XY(&x1, &y1);
    if (flag == 0)
        return (0);
    flag = TP_Read_XY(&x2, &y2);
    if (flag == 0)
        return (0);
    if (((x2 <= x1 && x1 < x2 + ERR_RANGE) || (x1 <= x2 && x2 < x1 + ERR_RANGE)) // 前后两次采样在+-50内
        && ((y2 <= y1 && y1 < y2 + ERR_RANGE) || (y1 <= y2 && y2 < y1 + ERR_RANGE)))
    {
        *x = (x1 + x2) / 2;
        *y = (y1 + y2) / 2;
        return 1;
    }
    else
        return 0;
}
//////////////////////////////////////////////////////////////////////////////////
// 与LCD部分有关的函数
// 画一个触摸点
// 用来校准用的
// x,y:坐标
// color:颜色
void TP_Drow_Touch_Point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_draw_line(x - 12, y, x + 13, y, color); // 横线
    lcd_draw_line(x, y - 12, x, y + 13, color); // 竖线
    lcd_draw_point(x + 1, y + 1, color);
    lcd_draw_point(x - 1, y + 1, color);
    lcd_draw_point(x + 1, y - 1, color);
    lcd_draw_point(x - 1, y - 1, color);
    lcd_draw_circle(x, y, 6, color); // 画中心圈
}
// 画一个大点(2*2的点)
// x,y:坐标
// color:颜色
void TP_Draw_Big_Point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_draw_point(x, y, color); // 中心点
    lcd_draw_point(x + 1, y, color);
    lcd_draw_point(x, y + 1, color);
    lcd_draw_point(x + 1, y + 1, color);
}
//////////////////////////////////////////////////////////////////////////////////
// 触摸按键扫描
// tp:0,屏幕坐标;1,物理坐标(校准等特殊场合用)
// 返回值:当前触屏状态.
// 0,触屏无触摸;1,触屏有触摸
uint8_t TP_Scan(uint8_t tp)
{
    if (PEN == 0) // 有按键按下
    {
        if (tp)
            TP_Read_XY2(&tp_dev.x[0], &tp_dev.y[0]);      // 读取物理坐标
        else if (TP_Read_XY2(&tp_dev.x[0], &tp_dev.y[0])) // 读取屏幕坐标
        {
            tp_dev.x[0] = tp_dev.xfac * tp_dev.x[0] + tp_dev.xoff; // 将结果转换为屏幕坐标
            tp_dev.y[0] = tp_dev.yfac * tp_dev.y[0] + tp_dev.yoff;
        }
        if ((tp_dev.sta & TP_PRES_DOWN) == 0) // 之前没有被按下
        {
            tp_dev.sta = TP_PRES_DOWN | TP_CATH_PRES; // 按键按下
            tp_dev.x[4] = tp_dev.x[0];                // 记录第一次按下时的坐标
            tp_dev.y[4] = tp_dev.y[0];
        }
    }
    else
    {
        if (tp_dev.sta & TP_PRES_DOWN) // 之前是被按下的
        {
            tp_dev.sta &= ~(1 << 7); // 标记按键松开
        }
        else // 之前就没有被按下
        {
            tp_dev.x[4] = 0;
            tp_dev.y[4] = 0;
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;
        }
    }
    return tp_dev.sta & TP_PRES_DOWN; // 返回当前的触屏状态
}
//////////////////////////////////////////////////////////////////////////
// 保存在EEPROM里面的地址区间基址,占用13个字节(RANGE:SAVE_ADDR_BASE~SAVE_ADDR_BASE+12)
#define SAVE_ADDR_BASE 40
// 保存校准参数
// void tp_save_adjust_data(void)
//{
//	uint32_t temp;
//	//保存校正结果!
//	temp=tp_dev.xfac*100000000;//保存x校正因素
//     at24cxx_write_len_byte(SAVE_ADDR_BASE,temp,4);
//	temp=tp_dev.yfac*100000000;//保存y校正因素
//     at24cxx_write_len_byte(SAVE_ADDR_BASE+4,temp,4);
//	//保存x偏移量
//     at24cxx_write_len_byte(SAVE_ADDR_BASE+8,tp_dev.xoff,2);
//	//保存y偏移量
//	at24cxx_write_len_byte(SAVE_ADDR_BASE+10,tp_dev.yoff,2);
//	//保存触屏类型
//	at24cxx_write_one_byte(SAVE_ADDR_BASE+12,tp_dev.touchtype);
//	temp=0X0B;//标记校准过了
//	at24cxx_write_one_byte(SAVE_ADDR_BASE+13,temp);
// }
void tp_save_adjust_data(void)
{
    uint8_t *p = (uint8_t *)&tp_dev.xfac; /* 指向首地址 */

    /* p指向tp_dev.xfac的地址, p+4则是tp_dev.yfac的地址
     * p+8则是tp_dev.xoff的地址,p+10,则是tp_dev.yoff的地址
     * 总共占用12个字节(4个参数)
     * p+12用于存放标记电阻触摸屏是否校准的数据(0X0A)
     * 往p[12]写入0X0A. 标记已经校准过.
     */
    at24cxx_write(SAVE_ADDR_BASE, p, 12);              /* 保存12个字节数据(xfac,yfac,xc,yc) */
    at24cxx_write_one_byte(SAVE_ADDR_BASE + 12, 0X0A); /* 保存校准值 */
}

// 得到保存在EEPROM里面的校准值
// 返回值：1，成功获取数据
//         0，获取失败，要重新校准
// uint8_t TP_Get_Adjdata(void)
//{
//	uint32_t tempfac;
//	tempfac=at24cxx_read_one_byte(SAVE_ADDR_BASE+13);//读取标记字,看是否校准过！
//	if(tempfac==0X0B)//触摸屏已经校准过了
//	{
//		tempfac=at24cxx_read_len_byte(SAVE_ADDR_BASE,4);
//		tp_dev.xfac=(float)tempfac/100000000;//得到x校准参数
//		tempfac=at24cxx_read_len_byte(SAVE_ADDR_BASE+4,4);
//		tp_dev.yfac=(float)tempfac/100000000;//得到y校准参数
//	    //得到x偏移量
//		tp_dev.xoff=at24cxx_read_len_byte(SAVE_ADDR_BASE+8,2);
//  	    //得到y偏移量
//		tp_dev.yoff=at24cxx_read_len_byte(SAVE_ADDR_BASE+10,2);
//  		tp_dev.touchtype=at24cxx_read_one_byte(SAVE_ADDR_BASE+12);//读取触屏类型标记
//		if(tp_dev.touchtype)//X,Y方向与屏幕相反
//		{
//			CMD_RDX=0X90;
//			CMD_RDY=0XD0;
//		}else				   //X,Y方向与屏幕相同
//		{
//			CMD_RDX=0XD0;
//			CMD_RDY=0X90;
//		}
//		return 1;
//	}
//	return 0;
// }

uint8_t TP_Get_Adjdata(void)
{
    uint8_t *p = (uint8_t *)&tp_dev.xfac;
    uint8_t temp = 0;

    /* 由于我们是直接指向tp_dev.xfac地址进行保存的, 读取的时候,将读取出来的数据
     * 写入指向tp_dev.xfac的首地址, 就可以还原写入进去的值, 而不需要理会具体的数
     * 据类型. 此方法适用于各种数据(包括结构体)的保存/读取(包括结构体).
     */
    at24cxx_read(SAVE_ADDR_BASE, p, 12);               /* 读取12字节数据 */
    temp = at24cxx_read_one_byte(SAVE_ADDR_BASE + 12); /* 读取校准状态标记 */

    if (temp == 0X0A)
    {
        tp_dev.touchtype = at24cxx_read_one_byte(SAVE_ADDR_BASE + 12); // 读取触屏类型标记
        if (tp_dev.touchtype)                                          // X,Y方向与屏幕相反
        {
            CMD_RDX = 0X90;
            CMD_RDY = 0XD0;
        }
        else // X,Y方向与屏幕相同
        {
            CMD_RDX = 0XD0;
            CMD_RDY = 0X90;
        }
        return 1;
    }

    return 0;
}

void tp_draw_big_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_draw_point(x, y, color); /* 中心点 */
    lcd_draw_point(x + 1, y, color);
    lcd_draw_point(x, y + 1, color);
    lcd_draw_point(x + 1, y + 1, color);
}

// 提示字符串
char *TP_REMIND_MSG_TBL = "Please use the stylus click the cross on the screen.The cross will always move until the screen adjustment is completed.";

// 提示校准结果(各个参数)
void TP_Adj_Info_Show(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3, uint16_t fac)
{
    lcd_show_string(40, 160, lcddev.width, lcddev.height, 16, "x1:", RED);
    lcd_show_string(40 + 80, 160, lcddev.width, lcddev.height, 16, "y1:", RED);
    lcd_show_string(40, 180, lcddev.width, lcddev.height, 16, "x2:", RED);
    lcd_show_string(40 + 80, 180, lcddev.width, lcddev.height, 16, "y2:", RED);
    lcd_show_string(40, 200, lcddev.width, lcddev.height, 16, "x3:", RED);
    lcd_show_string(40 + 80, 200, lcddev.width, lcddev.height, 16, "y3:", RED);
    lcd_show_string(40, 220, lcddev.width, lcddev.height, 16, "x4:", RED);
    lcd_show_string(40 + 80, 220, lcddev.width, lcddev.height, 16, "y4:", RED);
    lcd_show_string(40, 240, lcddev.width, lcddev.height, 16, "fac is:", RED);
    lcd_show_num(40 + 24, 160, x0, 4, 16, RED);           // 显示数值
    lcd_show_num(40 + 24 + 80, 160, y0, 4, 16, RED);      // 显示数值
    lcd_show_num(40 + 24, 180, x1, 4, 16, RED);           // 显示数值
    lcd_show_num(40 + 24 + 80, 180, y1, 4, 16, RED);      // 显示数值
    lcd_show_num(40 + 24, 200, x2, 4, 16, RED);           // 显示数值
    lcd_show_num(40 + 24 + 80, 200, y2, 4, 16, RED);      // 显示数值
    lcd_show_num(40 + 24, 220, x3, 4, 16, RED);           // 显示数值
    lcd_show_num(40 + 24 + 80, 220, y3, 4, 16, RED);      // 显示数值
    lcd_show_num(40 + 56, lcddev.width, fac, 3, 16, RED); // 显示数值,该数值必须在95~105范围之内.
}

// 触摸屏校准代码
// 得到四个校准参数
void tp_adjust(void)
{
    uint16_t pos_temp[4][2]; // 坐标缓存值
    uint8_t cnt = 0;
    uint16_t d1, d2;
    uint32_t tem1, tem2;
    float fac;
    uint16_t outtime = 0;
    cnt = 0;
    lcd_clear(WHITE); // 清屏

    lcd_show_string(40, 40, 160, 100, 16, TP_REMIND_MSG_TBL, BLACK); // 显示提示信息
    TP_Drow_Touch_Point(20, 20, RED);                                // 画点1
    tp_dev.sta = 0;                                                  // 消除触发信号
    tp_dev.xfac = 0;                                                 // xfac用来标记是否校准过,所以校准之前必须清掉!以免错误
    while (1)                                                        // 如果连续10秒钟没有按下,则自动退出
    {
        tp_dev.scan(1);                          // 扫描物理坐标
        if ((tp_dev.sta & 0xc0) == TP_CATH_PRES) // 按键按下了一次(此时按键松开了.)
        {
            outtime = 0;
            tp_dev.sta &= ~(1 << 6); // 标记按键已经被处理过了.

            pos_temp[cnt][0] = tp_dev.x[0];
            pos_temp[cnt][1] = tp_dev.y[0];
            cnt++;
            switch (cnt)
            {
            case 1:
                TP_Drow_Touch_Point(20, 20, WHITE);              // 清除点1
                TP_Drow_Touch_Point(lcddev.width - 20, 20, RED); // 画点2
                break;
            case 2:
                TP_Drow_Touch_Point(lcddev.width - 20, 20, WHITE); // 清除点2
                TP_Drow_Touch_Point(20, lcddev.height - 20, RED);  // 画点3
                break;
            case 3:
                TP_Drow_Touch_Point(20, lcddev.height - 20, WHITE);              // 清除点3
                TP_Drow_Touch_Point(lcddev.width - 20, lcddev.height - 20, RED); // 画点4
                break;
            case 4: // 全部四个点已经得到
                // 对边相等
                tem1 = abs(pos_temp[0][0] - pos_temp[1][0]); // x1-x2
                tem2 = abs(pos_temp[0][1] - pos_temp[1][1]); // y1-y2
                tem1 *= tem1;
                tem2 *= tem2;
                d1 = sqrt(tem1 + tem2); // 得到1,2的距离

                tem1 = abs(pos_temp[2][0] - pos_temp[3][0]); // x3-x4
                tem2 = abs(pos_temp[2][1] - pos_temp[3][1]); // y3-y4
                tem1 *= tem1;
                tem2 *= tem2;
                d2 = sqrt(tem1 + tem2); // 得到3,4的距离
                fac = (float)d1 / d2;
                if (fac < 0.95 || fac > 1.05 || d1 == 0 || d2 == 0) // 不合格
                {
                    cnt = 0;
                    TP_Drow_Touch_Point(lcddev.width - 20, lcddev.height - 20, WHITE); // 清除点4
                    TP_Drow_Touch_Point(20, 20, RED);                                  // 画点1
                    TP_Adj_Info_Show(pos_temp[0][0], pos_temp[0][1], pos_temp[1][0], pos_temp[1][1], pos_temp[2][0], pos_temp[2][1], pos_temp[3][0], pos_temp[3][1],
                                     fac * 100); // 显示数据
                    continue;
                }
                tem1 = abs(pos_temp[0][0] - pos_temp[2][0]); // x1-x3
                tem2 = abs(pos_temp[0][1] - pos_temp[2][1]); // y1-y3
                tem1 *= tem1;
                tem2 *= tem2;
                d1 = sqrt(tem1 + tem2); // 得到1,3的距离

                tem1 = abs(pos_temp[1][0] - pos_temp[3][0]); // x2-x4
                tem2 = abs(pos_temp[1][1] - pos_temp[3][1]); // y2-y4
                tem1 *= tem1;
                tem2 *= tem2;
                d2 = sqrt(tem1 + tem2); // 得到2,4的距离
                fac = (float)d1 / d2;
                if (fac < 0.95 || fac > 1.05) // 不合格
                {
                    cnt = 0;
                    TP_Drow_Touch_Point(lcddev.width - 20, lcddev.height - 20, WHITE); // 清除点4
                    TP_Drow_Touch_Point(20, 20, RED);                                  // 画点1
                    TP_Adj_Info_Show(pos_temp[0][0], pos_temp[0][1], pos_temp[1][0], pos_temp[1][1], pos_temp[2][0], pos_temp[2][1], pos_temp[3][0], pos_temp[3][1],
                                     fac * 100); // 显示数据
                    continue;
                } // 正确了

                // 对角线相等
                tem1 = abs(pos_temp[1][0] - pos_temp[2][0]); // x1-x3
                tem2 = abs(pos_temp[1][1] - pos_temp[2][1]); // y1-y3
                tem1 *= tem1;
                tem2 *= tem2;
                d1 = sqrt(tem1 + tem2); // 得到1,4的距离

                tem1 = abs(pos_temp[0][0] - pos_temp[3][0]); // x2-x4
                tem2 = abs(pos_temp[0][1] - pos_temp[3][1]); // y2-y4
                tem1 *= tem1;
                tem2 *= tem2;
                d2 = sqrt(tem1 + tem2); // 得到2,3的距离
                fac = (float)d1 / d2;
                if (fac < 0.95 || fac > 1.05) // 不合格
                {
                    cnt = 0;
                    TP_Drow_Touch_Point(lcddev.width - 20, lcddev.height - 20, WHITE); // 清除点4
                    TP_Drow_Touch_Point(20, 20, RED);                                  // 画点1
                    TP_Adj_Info_Show(pos_temp[0][0], pos_temp[0][1], pos_temp[1][0], pos_temp[1][1], pos_temp[2][0], pos_temp[2][1], pos_temp[3][0], pos_temp[3][1],
                                     fac * 100); // 显示数据
                    continue;
                } // 正确了
                // 计算结果
                tp_dev.xfac = (float)(lcddev.width - 40) / (pos_temp[1][0] - pos_temp[0][0]);       // 得到xfac
                tp_dev.xoff = (lcddev.width - tp_dev.xfac * (pos_temp[1][0] + pos_temp[0][0])) / 2; // 得到xoff

                tp_dev.yfac = (float)(lcddev.height - 40) / (pos_temp[2][1] - pos_temp[0][1]);       // 得到yfac
                tp_dev.yoff = (lcddev.height - tp_dev.yfac * (pos_temp[2][1] + pos_temp[0][1])) / 2; // 得到yoff
                if (fabs(tp_dev.xfac) > 2 || fabs(tp_dev.yfac) > 2)                                    // 触屏和预设的相反了.
                {
                    cnt = 0;
                    TP_Drow_Touch_Point(lcddev.width - 20, lcddev.height - 20, WHITE); // 清除点4
                    TP_Drow_Touch_Point(20, 20, RED);                                  // 画点1
                    lcd_show_string(40, 26, lcddev.width, lcddev.height, 16, "TP Need readjust!", RED);
                    tp_dev.touchtype = !tp_dev.touchtype; // 修改触屏类型.
                    if (tp_dev.touchtype)                 // X,Y方向与屏幕相反
                    {
                        CMD_RDX = 0X90;
                        CMD_RDY = 0XD0;
                    }
                    else // X,Y方向与屏幕相同
                    {
                        CMD_RDX = 0XD0;
                        CMD_RDY = 0X90;
                    }
                    continue;
                }
                lcd_clear(WHITE);                                                                          // 清屏
                lcd_show_string(35, 110, lcddev.width, lcddev.height, 16, "Touch Screen Adjust OK!", RED); // 校正完成
                delay_ms(1000);
                tp_save_adjust_data();
                lcd_clear(WHITE); // 清屏
                return;           // 校正完成
            }
        }
        delay_ms(10);
        outtime++;
        if (outtime > 1000)
        {
            TP_Get_Adjdata();
            break;
        }
    }
}
// 触摸屏初始化
// 返回值:0,没有进行校准
//        1,进行过校准
uint8_t TP_Init(void)
{
    GPIO_InitTypeDef GPIO_Initure;

    __HAL_RCC_GPIOB_CLK_ENABLE(); // 开启GPIOB时钟
    __HAL_RCC_GPIOF_CLK_ENABLE(); // 开启GPIOF时钟

    // PB1
    GPIO_Initure.Pin = GPIO_PIN_1 | GPIO_PIN_2; // PB1
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP;    // 推挽输出
    GPIO_Initure.Pull = GPIO_PULLUP;            // 上拉
    GPIO_Initure.Speed = GPIO_SPEED_FREQ_HIGH;  // 高速
    HAL_GPIO_Init(GPIOB, &GPIO_Initure);

    // PB2
    //    GPIO_Initure.Pin=GPIO_PIN_2; 				//PB2
    //    GPIO_Initure.Mode=GPIO_MODE_INPUT;  		//上拉输入
    //    HAL_GPIO_Init(GPIOB,&GPIO_Initure);

    // PF9,11
    GPIO_Initure.Pin = GPIO_PIN_9;           // PF9,11
    GPIO_Initure.Mode = GPIO_MODE_OUTPUT_PP; // 推挽输出
    HAL_GPIO_Init(GPIOF, &GPIO_Initure);

    // PF10
    GPIO_Initure.Pin = GPIO_PIN_10 | GPIO_PIN_8; // PF10
    GPIO_Initure.Mode = GPIO_MODE_INPUT;         // 输入
    GPIO_Initure.Pull = GPIO_PULLUP;             // 上拉
    HAL_GPIO_Init(GPIOF, &GPIO_Initure);

    TP_Read_XY(&tp_dev.x[0], &tp_dev.y[0]); // 第一次读取初始化
    at24cxx_init();                         // 初始化24CXX
    if (TP_Get_Adjdata())
        return 0; // 已经校准
    else          // 未校准?
    {
        lcd_clear(WHITE); // 清屏
        tp_adjust();      // 屏幕校准
    }
    TP_Get_Adjdata();
    return 1;
}

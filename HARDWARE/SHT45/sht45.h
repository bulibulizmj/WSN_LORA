#ifndef __SHT45_H
#define __SHT45_H
#include "sys.h"
#include "stdbool.h"
#include "myiic.h"
#include "delay.h"
/********************************************************************************	 
 * 本程序只供学习使用，未经作者许可，不得用于其它任何用途
 * ATKflight飞控固件
 * BMP280驱动代码	
 * 正点原子@ALIENTEK
 * 技术论坛:www.openedv.com
 * 创建日期:2018/5/2
 * 版本：V1.0
 * 版权所有，盗版必究。
 * Copyright(C) 广州市星翼电子科技有限公司 2014-2024
 * All rights reserved
********************************************************************************/



unsigned char cal_table_high_first(unsigned char value);
unsigned char crc_high_first(unsigned char *ptr, unsigned char len);
uint8_t SHT45_ReadPdata(uint8_t ack,float* T,float* H );
extern uint32_t SHT45_Data1[2];
void sht45init(void);
uint32_t SHT45_ReadRawData(uint8_t ack);
#endif



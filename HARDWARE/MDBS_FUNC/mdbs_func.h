#ifndef __MDBS_FUNC_H
#define __MDBS_FUNC_H
#include "stdio.h"	
#include "stm32f4xx_conf.h"
#include "sys.h" 
#include "Mb_usart.h"
#include "crc16.h"
#include "delay.h"
#include "stdbool.h"

//sp3485控制端口
 #define Mdbus_CTRL PBout(6)	// 拉高发送数据，拉低接收数据
//#define LED1 PCout(0)	// DS1	 
#define res_OK 0
#define res_ERR1 1
#define res_ERR2 2
#define res_ERR3 3
#define res_CRCERR 4


u8 mb_recv_readHoldingReg( _mbdata_st *mbp);
void mb_sent_writeHoldingReg( const _mbdata_st  mbp);
void Mdbus_CTRL_Init(void);//初始化	
void mb_sent_writeHoldingReg_all( const _mbdata_st mbp, u8 functioncode);
u8 mb_recv_readHoldingReg_all( _mbdata_st *mbp, u8 functioncode);
u16 CurrentPrecipitation(void);
bool CleanPrecipitation(void);
bool ModifyAddress(u8 Previousadd, u8 Currentadd);
u32 CurrentSoilstate(u8 DeviceAdd);
u16 CurrentWindsSpeed(void);
u16 CurrentWindsDirection(void);
void CurrentAddress(void);
u16 CurrentRadiation(void);

#endif



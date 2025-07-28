#ifndef _TIMER_H
#define _TIMER_H
#include "sys.h"
#include "stm32f4xx_conf.h"
#include "stdio.h"	
#include "Mb_usart.h"
#include <stdbool.h>
//通用定时器3中断初始化
//arr：自动重装值。
//psc：时钟预分频数
//定时器溢出时间计算方法:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=定时器工作频率,单位:Mhz
//这里使用的是定时器3!
void TIM3_Init(u16 psc, u16 arr);
void TIM2_Init(u16 psc, u16 arr);
void ConfigureTimeForRunTimeStats(void);

void mb_sent_writeHoldingReg( const _mbdata_st  mbp);
u8 mb_recv_readHoldingReg( _mbdata_st *mbp);
u8 mb_recv_writeHoldingReg( _mbdata_st  *mbp );
void mb_setMODRXorTX(bool RxorTx);
void smb_sentHoldingReg(const _mbdata_st  mbp   );
 u8 smb_recvHoldingReg( _mbdata_st  *mbp   );
#endif



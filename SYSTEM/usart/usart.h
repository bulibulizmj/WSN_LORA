#ifndef __USART_H
#define __USART_H
#include "stdio.h"	
#include "stm32f4xx_conf.h"
#include "sys.h" 

////////////////////////////////////////////////////////////////////////////////// 	
#define BUFLEN            1024
#define USART_REC_LEN  		200  	//定义最大接收字节数 200
#define EN_USART1_RX 			1		  //使能（1）/禁止（0）串口1接收
	  	

extern u8  USART_RX_BUF[USART_REC_LEN]; //接收缓冲,最大USART_REC_LEN个字节.末字节为换行符 
extern u16 USART_RX_STA;         		    //接收状态标记	

extern volatile u32 g_usart1_rx_overflow;
extern volatile u32 g_usart2_rx_overflow;
extern volatile u32 g_uart4_rx_overflow;


//如果想串口中断接收，请不要注释以下宏定义
void uart_init(u32 bound);
void uart2_init(u32 bound);
void uart4_init(u32 bound);
void Uart2_SendStr(char*SendBuf);
void Uart4_SendStr(char*SendBuf);
 
#endif





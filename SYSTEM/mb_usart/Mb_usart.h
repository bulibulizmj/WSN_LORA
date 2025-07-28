#ifndef __MBUSART_H
#define __MBUSART_H
#include "stdio.h"	
#include "stm32f4xx_conf.h"
#include "sys.h" 


/*DMA接收数据缓存大小*/
#define UART_DMARX_SIZE 0xff
 
typedef struct  
{
    u8 buf[UART_DMARX_SIZE];
    __IO u8 len;
} _serialbuf_st ;  //串口数据结构
 
typedef struct  
{
    u8 addr;//从机地址
    u16 start;//寄存器起始
    u8 len;  //接收到或待发送的寄存器数
    u16 buf[UART_DMARX_SIZE/2];//寄存器数据
} _mbdata_st; //用户数据
 
extern _serialbuf_st serialRXbuf_st;
extern _serialbuf_st serialTXbuf_st;
 
void Usart3_init(u32 baud) ;
void WaitForTransmitComplete(USART_TypeDef* USARTx) ;
void myUSART_Sendbyte(USART_TypeDef* USARTx, uint16_t Data) ;
void myUSART_Sendstr(USART_TypeDef* USARTx, const char  *s) ;
void myUSART_Sendarr(USART_TypeDef* USARTx, u8 a[] ,uint8_t len);
 
#endif



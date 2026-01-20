#include "sys.h"
#include "usart.h"	
#include "mac.h"
#include "routing.h"

u8 AtRxBuffer_EC800[200];
u16 Rxcouter_EC800;

u8 lora_serialRXbuf_st[LORA_SERIAL_BUF_SIZE];
u16 lora_Rxcouter;
volatile u8 lora_frame_locked = 0; // LoRa frame lock (1=ready, stop writing RX buffer)
volatile u8 g_lora_rx_framing_enable = 0; // 0=raw stream (AT), 1=SOF+LEN framed packets

/* LoRa UART RX framing (SOF+LEN) state machine */
#define LORA_RX_WAIT_RSSI  0u
#define LORA_RX_WAIT_SOF0  1u
#define LORA_RX_WAIT_SOF1  2u
#define LORA_RX_WAIT_LEN   3u
#define LORA_RX_RECV_DATA  4u

static volatile u8  g_lora_rx_state = LORA_RX_WAIT_RSSI;
static volatile u16 g_lora_rx_expected_total = 0;
////////////////////////////////////////////////////////////////////////////////// 	 
//如果使用ucos,则包括下面的头文件即可.
#if SYSTEM_SUPPORT_OS
#include "FreeRTOS.h"					//FreeRTOS 使用
#include "event_groups.h" 
#endif

extern EventGroupHandle_t recv_eventgroup_handle;		//接收事件标志组句柄
extern EventBits_t recv_eventgroup_bit;

extern EventGroupHandle_t route_eventgroup_handle;		//路由层事件标志组句柄
extern EventBits_t route_eventgroup_bit;
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F4探索者开发板
//串口1初始化		   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2014/6/10
//版本：V1.5
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved
//********************************************************************************
//V1.3修改说明 
//支持适应不同频率下的串口波特率设置.
//加入了对printf的支持
//增加了串口接收命令功能.
//修正了printf第一个字符丢失的bug
//V1.4修改说明
//1,修改串口初始化IO的bug
//2,修改了USART_RX_STA,使得串口最大接收字节数为2的14次方
//3,增加了USART_REC_LEN,用于定义串口最大允许接收的字节数(不大于2的14次方)
//4,修改了EN_USART1_RX的使能方式
//V1.5修改说明
//1,增加了对UCOSII的支持
////////////////////////////////////////////////////////////////////////////////// 	  
 

//////////////////////////////////////////////////////////////////
//加入以下代码,支持printf函数,而不需要选择use MicroLIB	  
#if 1
#pragma import(__use_no_semihosting)             
//标准库需要的支持函数                 
struct __FILE 
{ 
	int handle; 
}; 

FILE __stdout;       
//定义_sys_exit()以避免使用半主机模式    
void _sys_exit(int x) 
{ 
	x = x; 
} 
//重定义fputc函数 
int fputc(int ch, FILE *f)
{ 	
	while((USART1->SR&0X40)==0);//循环发送,直到发送完毕   
	USART1->DR = (u8) ch;      
	return ch;
}
#endif
 
#if EN_USART1_RX   //如果使能了接收
//串口1中断服务程序
//注意,读取USARTx->SR能避免莫名其妙的错误   	
u8 USART_RX_BUF[USART_REC_LEN];     //接收缓冲,最大USART_REC_LEN个字节.



//接收状态
//bit15，	接收完成标志
//bit14，	接收到0x0d
//bit13~0，	接收到的有效字节数目
u16 USART_RX_STA=0;       //接收状态标记	

//初始化IO 串口1 
//bound:波特率
void uart_init(u32 bound){
   //GPIO端口设置
  GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE); //使能GPIOA时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1,ENABLE);//使能USART1时钟
 
	//串口1对应引脚复用映射
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource9,GPIO_AF_USART1); //GPIOA9复用为USART1
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource10,GPIO_AF_USART1); //GPIOA10复用为USART1
	
	//USART1端口配置
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10; //GPIOA9与GPIOA10
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	//速度50MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽复用输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉
	GPIO_Init(GPIOA,&GPIO_InitStructure); //初始化PA9，PA10

   //USART1 初始化设置
	USART_InitStructure.USART_BaudRate = bound;//波特率设置
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
  USART_Init(USART1, &USART_InitStructure); //初始化串口1
	
  USART_Cmd(USART1, ENABLE);  //使能串口1 
	
	//USART_ClearFlag(USART1, USART_FLAG_TC);
	
#if EN_USART1_RX	
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);//开启相关中断

	//Usart1 NVIC 配置
  NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;//串口1中断通道
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=6;//抢占优先级3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =0;		//子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器、

#endif
	
}

void uart2_init(u32 bound)
{
    //GPIO端口设置
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA,ENABLE); //使能GPIOA时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);//使能USART2时钟

    //串口1对应引脚复用映射
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource2,GPIO_AF_USART2); //GPIOA2复用为USART2
    GPIO_PinAFConfig(GPIOA,GPIO_PinSource3,GPIO_AF_USART2); //GPIOA3复用为USART2

    //USART1端口配置
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3; //GPIOA2与GPIOA3
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	//速度50MHz
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽复用输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉
    GPIO_Init(GPIOA,&GPIO_InitStructure); //初始化PA2，PA3

    //USART1 初始化设置
    USART_InitStructure.USART_BaudRate = bound;//波特率设置
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
    USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
    USART_Init(USART2, &USART_InitStructure); //初始化串口2

    USART_Cmd(USART2, ENABLE);  //使能串口2 

    USART_ClearFlag(USART2, USART_FLAG_TC);

    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);//开启相关中断

    //Usart2 NVIC 配置
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;//串口2中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;//抢占优先级1
    NVIC_InitStructure.NVIC_IRQChannelSubPriority =0;		//子优先级3
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
    NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器、
}




void USART1_IRQHandler(void)                	//串口1中断服务程序，  * @note   RXNE pending bit can be also cleared by a read to the USART_DR register (USART_ReceiveData()).
{
    u8 Res;
		BaseType_t xHigherPriorityTaskWoken, xResult;
		xHigherPriorityTaskWoken = pdFALSE;
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)  //接收中断
    {
        Res = USART_ReceiveData(USART1);  //读取接收到的数据
        USART_RX_BUF[USART_RX_STA++]=Res;//
    }
    xResult = xEventGroupSetBitsFromISR(route_eventgroup_handle, WRITE_ADDR_ORDER, &xHigherPriorityTaskWoken);
    if(xResult == pdPASS)//是否导致有高优先级任务就绪？如果有则进行任务切换
    {
        portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
    }
    USART_ClearITPendingBit(USART1, USART_IT_RXNE);
} 

//串口2的接收中断函数
void USART2_IRQHandler(void)                                //串口2中断服务程序
{
    u8 Res;
    if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)  //接收中断
    {
        Res = USART_ReceiveData(USART2);//(USART1->DR);      //读取接收到的数据
        AtRxBuffer_EC800[Rxcouter_EC800++]=Res;//
    } 

}

//串口2的发送函数
void Uart2_SendStr(char*SendBuf)    //串口2打印数据
{
    while(*SendBuf)
    {
        while((USART2->SR&0X40)==0);//等待发送完成
        USART2->DR = (u8) *SendBuf;
        SendBuf++;
    }
}

void uart4_init(u32 bound)
{
    //GPIO端口设置
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE); //使能GPIOA时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4,ENABLE);//使能USART2时钟

    //串口1对应引脚复用映射
    GPIO_PinAFConfig(GPIOC,GPIO_PinSource10,GPIO_AF_UART4); //GPIOA2复用为USART2
    GPIO_PinAFConfig(GPIOC,GPIO_PinSource11,GPIO_AF_UART4); //GPIOA3复用为USART2

    //USART1端口配置
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11; //GPIOA2与GPIOA3
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	//速度50MHz
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽复用输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉
    GPIO_Init(GPIOC,&GPIO_InitStructure); //初始化PA2，PA3

    //USART1 初始化设置
    USART_InitStructure.USART_BaudRate = bound;//波特率设置
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
    USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
    USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
    USART_Init(UART4, &USART_InitStructure); //初始化串口2

    USART_Cmd(UART4, ENABLE);  //使能串口2 

    USART_ClearFlag(UART4, USART_FLAG_TC);

    USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);//开启相关中断

    //Usart2 NVIC 配置
    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;//串口2中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=7;//抢占优先级1
    NVIC_InitStructure.NVIC_IRQChannelSubPriority =0;		//子优先级3
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//IRQ通道使能
    NVIC_Init(&NVIC_InitStructure);	//根据指定的参数初始化VIC寄存器、

}

//串口4的接收中断函数
void UART4_IRQHandler(void)                                
{
    u8 Res;
    u8 frame_complete = 0;
    EventBits_t bits_to_set = 0;
    BaseType_t xHigherPriorityTaskWoken, xResult;
    xHigherPriorityTaskWoken = pdFALSE;

    if (USART_GetITStatus(UART4, USART_IT_RXNE) != RESET)  //接收中断
    {
        Res = USART_ReceiveData(UART4); //读取接收到的数据

        if (g_lora_rx_framing_enable == 0)
        {
            lora_frame_locked = 0;
            g_lora_rx_state = LORA_RX_WAIT_RSSI;
            g_lora_rx_expected_total = 0;

            if (lora_Rxcouter < (u16)(sizeof(lora_serialRXbuf_st) - 1u))
            {
                lora_serialRXbuf_st[lora_Rxcouter++] = Res;
                lora_serialRXbuf_st[lora_Rxcouter] = 0;
            }
            else
            {
                lora_Rxcouter = 0;
            }
        }
        else
        {
            /* When upper layer clears lora_Rxcouter to 0, unlock RX buffer and reset state. */
            if ((lora_frame_locked != 0) && (lora_Rxcouter == 0))
            {
                lora_frame_locked = 0;
                g_lora_rx_state = LORA_RX_WAIT_RSSI;
                g_lora_rx_expected_total = 0;
            }

            if (lora_frame_locked == 0)
            {
                switch (g_lora_rx_state)
                {
                    case LORA_RX_WAIT_RSSI:
                        lora_Rxcouter = 0;
                        lora_serialRXbuf_st[0] = Res; /* RSSI byte from module */
                        lora_Rxcouter = 1;
                        g_lora_rx_state = LORA_RX_WAIT_SOF0;
                        break;

                    case LORA_RX_WAIT_SOF0:
                        if (Res == (u8)LORA_FRAME_SOF0)
                        {
                            lora_serialRXbuf_st[1] = Res;
                            lora_Rxcouter = 2;
                            g_lora_rx_state = LORA_RX_WAIT_SOF1;
                        }
                        else
                        {
                            /* Not aligned: treat this byte as new RSSI. */
                            lora_serialRXbuf_st[0] = Res;
                            lora_Rxcouter = 1;
                        }
                        break;

                    case LORA_RX_WAIT_SOF1:
                        if (Res == (u8)LORA_FRAME_SOF1)
                        {
                            lora_serialRXbuf_st[2] = Res;
                            lora_Rxcouter = 3;
                            g_lora_rx_state = LORA_RX_WAIT_LEN;
                        }
                        else
                        {
                            lora_serialRXbuf_st[0] = Res;
                            lora_Rxcouter = 1;
                            g_lora_rx_state = LORA_RX_WAIT_SOF0;
                        }
                        break;

                    case LORA_RX_WAIT_LEN:
                        if ((Res < (u8)LORA_FRAME_DATA_MIN_LEN) || (Res > (u8)LORA_FRAME_DATA_MAX_LEN))
                        {
                            /* Invalid length, restart. */
                            lora_Rxcouter = 0;
                            g_lora_rx_expected_total = 0;
                            g_lora_rx_state = LORA_RX_WAIT_RSSI;
                        }
                        else
                        {
                            lora_serialRXbuf_st[3] = Res; /* LEN (includes CRC16) */
                            lora_Rxcouter = 4;
                            g_lora_rx_expected_total = (u16)(4u + (u16)Res);
                            g_lora_rx_state = LORA_RX_RECV_DATA;
                        }
                        break;

                    case LORA_RX_RECV_DATA:
                    default:
                        if (lora_Rxcouter < sizeof(lora_serialRXbuf_st))
                        {
                            lora_serialRXbuf_st[lora_Rxcouter++] = Res;
                            if ((g_lora_rx_expected_total != 0) && (lora_Rxcouter >= g_lora_rx_expected_total))
                            {
                                lora_Rxcouter = g_lora_rx_expected_total;
                                lora_frame_locked = 1;
                                frame_complete = 1;

                                g_lora_rx_state = LORA_RX_WAIT_RSSI;
                                g_lora_rx_expected_total = 0;
                            }
                        }
                        else
                        {
                            /* Overflow, restart. */
                            lora_Rxcouter = 0;
                            g_lora_rx_expected_total = 0;
                            g_lora_rx_state = LORA_RX_WAIT_RSSI;
                        }
                        break;
                }
            }
        }

        if ((recv_eventgroup_bit & INITIAL_OK) && (g_lora_rx_framing_enable != 0))
        {
            bits_to_set = CSMA_BUSY_7;
            if (frame_complete)
            {
                bits_to_set |= PACKET_RECV_2;
            }
            xResult = xEventGroupSetBitsFromISR(recv_eventgroup_handle, bits_to_set, &xHigherPriorityTaskWoken);
            if (xResult == pdPASS) //是否导致有高优先级任务就绪？如果有则进行任务切换
            {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        }
    }
}

//串口4的发送函数
void Uart4_SendStr(char* SendBuf)    //串口2打印数据
{
    while(*SendBuf)
    {
        while((UART4->SR&0X40)==0);//等待发送完成
        UART4->DR = (u8) *SendBuf;
        SendBuf++;
    }
}






#endif	

 




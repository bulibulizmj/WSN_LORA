 
#include "Mb_usart.h"
#include "string.h"
 
_serialbuf_st serialRXbuf_st;
_serialbuf_st serialTXbuf_st;
 
/*DMA接收数据缓存*/
u8 g_uart1DmaRXBuf[UART_DMARX_SIZE];
   
/*
说明：3个串口直接发送函数
编写：林
*/
void myUSART_Sendbyte(USART_TypeDef* USARTx, uint16_t Data)
{
    while((USARTx->SR&0X40)==0); 
    USARTx->DR = (Data & (uint16_t)0x01FF);
}
void myUSART_Sendstr(USART_TypeDef* USARTx, const char  *s)
{
    while(*s != '\0')
    {      
        myUSART_Sendbyte( USARTx, *s) ;
        s++;
    }
}
void myUSART_Sendarr(USART_TypeDef* USARTx, u8 a[] ,uint8_t len)
{
    uint8_t i=0;
    while(i <  len )
    {      
        myUSART_Sendbyte( USARTx, a[i]) ;
        i++;
    }
}
 
/*
说明：
  串口1初始化
  串口1使用DMA 接收 
编写：林
*/
void Usart3_init(u32 baud)
{    
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef  USART_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
 
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE); //使能GPIOA时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE);//使能USART3时钟
    //串口3对应引脚复用映射
    GPIO_PinAFConfig(GPIOB,GPIO_PinSource10,GPIO_AF_USART3); //GPIOB10复用为USART3
    GPIO_PinAFConfig(GPIOB,GPIO_PinSource11,GPIO_AF_USART3); //GPIOB11复用为USART3

    //USART3端口配置
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用功能
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;	//速度50MHz
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP; //推挽复用输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP; //上拉
    GPIO_Init(GPIOB,&GPIO_InitStructure); //初始化PB10，PB11


		 //USART3初始化设置
		USART_InitStructure.USART_BaudRate = baud;//波特率设置
		USART_InitStructure.USART_WordLength = USART_WordLength_8b;//字长为8位数据格式
		USART_InitStructure.USART_StopBits = USART_StopBits_1;//一个停止位
		USART_InitStructure.USART_Parity = USART_Parity_No;//无奇偶校验位
		USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//无硬件数据流控制
		USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	//收发模式
		USART_Init(USART3, &USART_InitStructure); //初始化串口1
		
		USART_Cmd(USART3, ENABLE);  //使能串口1 
	
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA1,ENABLE);//DMA1时钟使能 
		
    DMA_DeInit(DMA1_Stream1);
		while (DMA_GetCmdStatus(DMA1_Stream1) != DISABLE){}//等待DMA可配置 
			

			
		DMA_InitStructure.DMA_Channel = DMA_Channel_4;  //通道选择
		DMA_InitStructure.DMA_PeripheralBaseAddr = (u32)(&(USART3->DR));//DMA外设地址
		DMA_InitStructure.DMA_Memory0BaseAddr = (u32)g_uart1DmaRXBuf;//DMA 存储器0地址
		DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;//存储器到外设模式
		DMA_InitStructure.DMA_BufferSize = UART_DMARX_SIZE;//数据传输量 
		DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;//外设非增量模式
		DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;//存储器增量模式
		DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;//外设数据长度:8位
		DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;//存储器数据长度:8位
		DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;// 使用循环模式 ，只要串口接收到字节就开始搬移数据，非常重要！！！！！！！！！！不然第二次发送数据的时候产生不了DMA请求
		DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;//中等优先级
		DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;         
		DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
		DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;//存储器突发单次传输
		DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;//外设突发单次传输
		 DMA_Init(DMA1_Stream1,&DMA_InitStructure);  
 
    USART_ITConfig(USART3,USART_IT_TC,DISABLE);  //关闭中断
    USART_ITConfig(USART3,USART_IT_RXNE,DISABLE);  //当接收到1个字节，会产生USART_IT_RXNE中断，关闭
    USART_ITConfig(USART3,USART_IT_IDLE,ENABLE); //开启USART_IT_IDLE中断，即空闲中断检测，空闲中断是在监测到数据接收后（即串口的RXNE位被置位）开始检测，当总线上在一个字节对应的周期内未再有新的数据接收时，触发空闲中断IDLE位被硬件置1.
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;               //   
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;       //    
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;              //    
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                 //     
    NVIC_Init(&NVIC_InitStructure);     
                                            
    USART_DMACmd(USART3,USART_DMAReq_Rx,ENABLE);  
    USART_Cmd(USART3, ENABLE);     
    DMA_Cmd(DMA1_Stream1,ENABLE);  
 
    memset(   & serialRXbuf_st ,0, sizeof (   serialRXbuf_st ) ) ;//结构体清0
}



//等待发送完成
void WaitForTransmitComplete(USART_TypeDef* USARTx)
{
    while((USARTx->SR&0X40)==0){}; 
}
 
/*
说明：串口中断，DMA与空闲中断处理，用于串口接收
编写：林
*/
void USART3_IRQHandler(void)//接收总线空闲时进入一次中断，每接收一个字节产生一次DMA请求，NDTR计数器的值减1，temp的值则为已接收的字节数。
{ 
    _serialbuf_st *p= &serialRXbuf_st;
    __IO u8 temp = 0;
    u8 i=0;
 
    if(USART_GetITStatus(USART3, USART_IT_IDLE) != RESET)
    {
        temp = USART3->SR;//清中断
        temp = USART3->DR; 
        DMA_Cmd(DMA1_Stream1,DISABLE);
        temp = UART_DMARX_SIZE - ((uint16_t)(DMA1_Stream1->NDTR));//得到DMA传输数据的长度（字节个数）
			
			//测试用
//				printf("temp:%d\r\n",temp);
//				for (i = 0;i < 9;i++)
//        {
//							printf("no temp g_uart1DmaRXBuf[i]:%x\r\n",g_uart1DmaRXBuf[i]);//尝试清零这个数组
//        }
			
        for (i = 0;i < temp;i++)
        {
              p->buf[i] =g_uart1DmaRXBuf[i];//赋值到全局变量serialRXbuf_st->buf中
							//printf("g_uart1DmaRXBuf[i]:%x\r\n",g_uart1DmaRXBuf[i]);
        }
        p->len = temp ;//接收的字节个数
        
        DMA_SetCurrDataCounter(DMA1_Stream1,UART_DMARX_SIZE);//重置NDTR计数器
        DMA_Cmd(DMA1_Stream1,ENABLE);
    }
		GPIO_SetBits(GPIOC,GPIO_Pin_4);
    __nop(); 
}



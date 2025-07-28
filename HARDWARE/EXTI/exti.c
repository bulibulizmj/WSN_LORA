#include "exti.h"
#include "delay.h" 
#include "led.h" 
#include "key.h"
#include "FreeRTOS.h"
#include "task.h"


//外部中断0服务程序
void EXTI0_IRQHandler(void)
{
		printf("进入中断 \r\n");
		delay_xms(20);	//消抖
		if(WK_UP==1)	 
		{

		}
	


	 EXTI_ClearITPendingBit(EXTI_Line0); //清除LINE0上的中断标志位 
}	

	   
//外部中断初始化程序
//初始化PE2~4,PA0为中断输入.
void EXTIX_Init(void)
{
	NVIC_InitTypeDef   NVIC_InitStructure;
	EXTI_InitTypeDef   EXTI_InitStructure;
	
	KEY_Init(); //按键对应的IO口初始化
 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);//使能SYSCFG时钟
 
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);//PE4 连接到中断线4
	
	/* 配置EXTI_Line4 */
	EXTI_InitStructure.EXTI_Line =  EXTI_Line0;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;		//中断事件
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising; //下降沿触发
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;				//中断线使能
	EXTI_Init(&EXTI_InitStructure);							//配置
 
	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;		//外部中断4
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x06;//抢占优先级6
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;	//子优先级0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			//使能外部中断通道
	NVIC_Init(&NVIC_InitStructure);							//配置	   
}













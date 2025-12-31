#include "timer.h"
#include "ewdg.h"
uint32_t FreeRTOSRunTimeTicks;

void ConfigureTimeForRunTimeStats(void)
{
		TIM3_Init(10-1, 8400-1); /* 10us, 100倍的系统时钟节拍 */
		FreeRTOSRunTimeTicks = 0;
}

void TIM3_Init(u16 psc, u16 arr)
{
	TIM_TimeBaseInitTypeDef TIM3_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);//APB1定时器时钟为 84M,因为APB1 Prescaler为4，即总线时钟分频系数为4，168/4 = 42而定时器时钟在APB1总线分频系数不为1时的频率是APB1总线频率的两倍
	TIM3_InitStructure.TIM_Prescaler = psc;
	TIM3_InitStructure.TIM_Period = arr;//自动重装载值
	TIM3_InitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM3_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;//计数模式
	
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE); //允许定时器3更新中断
	
	TIM_TimeBaseInit(TIM3,&TIM3_InitStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 9;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_Cmd(TIM3,ENABLE);//使能定时器3
}

void TIM2_Init(u16 psc, u16 arr)
{
	TIM_TimeBaseInitTypeDef TIM2_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);//APB1定时器时钟为 84M,因为APB1 Prescaler为4，即总线时钟分频系数为4，168/4 = 42而定时器时钟在APB1总线分频系数不为1时的频率是APB1总线频率的两倍
	TIM2_InitStructure.TIM_Prescaler = psc;
	TIM2_InitStructure.TIM_Period = arr;//自动重装载值
	TIM2_InitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM2_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;//计数模式
	
	TIM_ITConfig(TIM2,TIM_IT_Update,ENABLE); //允许定时器3更新中断
	
	TIM_TimeBaseInit(TIM2,&TIM2_InitStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 4;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_Cmd(TIM2,ENABLE);//使能定时器3
}

void TIM3_IRQHandler(void)
{
	if(TIM_GetFlagStatus(TIM3,TIM_FLAG_Update) == 1)//溢出中断
	{
			FreeRTOSRunTimeTicks ++;
	}
	TIM_ClearITPendingBit(TIM3,TIM_IT_Update);  //清除中断标志位
}

void TIM2_IRQHandler(void)
{
	if(TIM_GetFlagStatus(TIM2,TIM_FLAG_Update) == 1)//溢出中断
	{
			EWDG_Feed(); // 喂外部看门狗
	}
	TIM_ClearITPendingBit(TIM2,TIM_IT_Update);  //清除中断标志位
}


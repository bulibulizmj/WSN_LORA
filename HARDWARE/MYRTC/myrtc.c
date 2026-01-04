#include "stm32f4xx.h"                  
#include "myrtc.h"
#include "delay.h"
#include "usart.h"
#include "LED.h"

uint32_t overflow_count = 0;
const uint32_t target_overflows = 2; // 溢出次数，WAKE_UP_SECONDS为3600时每1小时溢出一次

//时间设置函数
//hour，min，sec 小时 分钟 秒
//ampm：可供选择的范围为 RTC_H12_AM/RTC_H12_PM
//返回值：SUCCED(1)成功
//		  ERROR(0)失败
ErrorStatus RTC_Set_Time(u8 hour,u8 min,u8 sec,u8 ampm)
{
	RTC_TimeTypeDef RTC_TimeTypeInitStructure;
	
	RTC_TimeTypeInitStructure.RTC_H12=ampm;
	RTC_TimeTypeInitStructure.RTC_Hours=hour;
	RTC_TimeTypeInitStructure.RTC_Minutes=min;
	RTC_TimeTypeInitStructure.RTC_Seconds=sec;
	
	return RTC_SetTime(RTC_Format_BIN,&RTC_TimeTypeInitStructure);
	//参数设置相关时间，把建立的时间SetTime返回，供主函数GetTime获取
}
 
//日期设置函数
//year，month，date：年月日
//week：星期1~7有效 0非法
//返回值：SUCCED(1)成功
//		  ERROR(0)失败
ErrorStatus RTC_Set_Date(u8 year,u8 month,u8 date,u8 week)
{
	RTC_DateTypeDef RTC_DateTypeInitStructure;
	
	RTC_DateTypeInitStructure.RTC_Date=date;
	RTC_DateTypeInitStructure.RTC_Month=month;
	RTC_DateTypeInitStructure.RTC_WeekDay=week;
	RTC_DateTypeInitStructure.RTC_Year=year;
	
	return RTC_SetDate(RTC_Format_BIN,&RTC_DateTypeInitStructure);
}
 
//RTC初始化
//返回值：0初始化成功
//		  1 LSE开启失败
//板子无LSE 采用LSI
u8 My_RTC_Init(void)
{
	u8 retry=0;
	u16 ck = 0;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR,ENABLE);//使能PWR时钟
	PWR_BackupAccessCmd(ENABLE);//使能后备寄存器
	//RCC_LSICmd(ENABLE);
	RCC_LSEConfig(RCC_LSE_ON);//LSE开始，打开时钟
	delay_ms(20);
	while(RCC_GetFlagStatus(RCC_FLAG_LSERDY)==RESET)//while循环判断LSE开启的状态位是否置1
		//显然while循环离开的标志是获取的状态位RCC_FLAG_LSERDY等于1，离开也就代表这LSE开启成功
	{
		retry++;
		delay_ms(10);
		RCC_LSEConfig(RCC_LSE_ON);//LSE开始，打开时钟
		if(retry==100) return 1;//LSE开启失败
	}

//	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);//既然程序没有返回1能来到这，一定表示LSE开启成功了，那么设置LSE作为RTC时钟
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);//既然程序没有返回1能来到这，一定表示LSE开启成功了，那么设置LSE作为RTC时钟
	RCC_RTCCLKCmd(ENABLE);//设置完RTC时钟以后，使能该时钟
	RTC_WaitForSynchro();
	ck = RTC_ReadBackupRegister(RTC_BKP_DR0);
//	printf("RTC_BKP_DR0 = %x\r\n",ck);
	if(ck!=0x9528)//判断是否为第一次配置，这里的0x5051完全是自己设置的一个标志位
		//RTC_ReadBackupRegister叫做读取后备寄存器，RTC_BKP_DR0为寄存器的标志位，如果读取标志位不为我们设置的0x5051
	//则表示是第一次配置，执行下述程序；如果不是第一次配置，断电从启后会跳过该代码
	{

		RTC_InitTypeDef RTC_InitStructure;
		
		RTC_InitStructure.RTC_AsynchPrediv=0x7F;//异步预分频系数七位有效 1~0x7F 0x7D = 125
		RTC_InitStructure.RTC_HourFormat=RTC_HourFormat_24; //24小时格式
		RTC_InitStructure.RTC_SynchPrediv=0xFF;//同步预分频系数15位有效 0~7FFF
		
		RTC_Init(&RTC_InitStructure);  //RTC_Init是库函数定义好的结构体，直接调用即可，也正因为如此，我们的初始化函数不可以设置为RTC_Init
		
		RTC_Set_Time(15,38,50,RTC_H12_AM);//根据上面自己设置的函数，设置时间，因为这个是第一次配置才会调用该程序，所以SetTime会供主函数GetTime调用
		RTC_Set_Date(24,7,11,4);  	
		
		RTC_WriteBackupRegister(RTC_BKP_DR0,0x9528);//程序能来到这里，表示上面的初始化已经完成了，并且是第一次配置的，这里将后备寄存器的状态位设置为0x5051
		//断电重启以后，不在重复执行该程序；0x5051对应if判断中设置的状态位；
		//在这里，如果想要每次重启以后都能执行上述时间，可以通过每次都设置新的状态位值来实现
	}
	return 0;//初始化成功
}
 
////设置闹钟时间
////week：星期1~7有效 
////hour，min，sec 小时 分钟秒
//void RTC_Set_AlarmA(u8 week,u8 hour,u8 min,u8 sec)//STM32实时时钟具有2个可编程闹钟A和B，这里使用闹钟A，闹钟B只要设置相应寄存器的相关位即可
//{
//	RTC_AlarmCmd(RTC_Alarm_A,DISABLE);//关闭闹钟A
//	
//	RTC_TimeTypeDef RTC_TimeTypeInitStructure;
//	RTC_TimeTypeInitStructure.RTC_H12=RTC_H12_AM;
//	RTC_TimeTypeInitStructure.RTC_Hours=hour;
//	RTC_TimeTypeInitStructure.RTC_Minutes=min;
//	RTC_TimeTypeInitStructure.RTC_Seconds=sec;
//	
//	RTC_AlarmTypeDef RTC_AlarmTypeInitStructure;
//	RTC_AlarmTypeInitStructure.RTC_AlarmDateWeekDay=week;//星期
//	RTC_AlarmTypeInitStructure.RTC_AlarmDateWeekDaySel=RTC_AlarmDateWeekDaySel_WeekDay;//按星期闹
//	RTC_AlarmTypeInitStructure.RTC_AlarmMask=RTC_AlarmMask_None;//精确匹配到星期 时分秒
//	RTC_AlarmTypeInitStructure.RTC_AlarmTime=RTC_TimeTypeInitStructure;
//	RTC_SetAlarm(RTC_Format_BIN,RTC_Alarm_A,&RTC_AlarmTypeInitStructure);//设置闹钟参数
//	
//	RTC_ClearITPendingBit(RTC_IT_ALRA);//清除RTC闹钟A的标志
//	EXTI_ClearITPendingBit(EXTI_Line17);//清除LINE17上的中断标志位
//	
//	RTC_ITConfig(RTC_IT_ALRA,ENABLE);//开启闹钟A中断
//	RTC_AlarmCmd(RTC_Alarm_A,ENABLE);//开启闹钟A
//	
//	EXTI_InitTypeDef EXTI_InitStructure;
//	EXTI_InitStructure.EXTI_Line=EXTI_Line17;//LINE17
//	EXTI_InitStructure.EXTI_LineCmd=ENABLE;//使能
//	EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;//中断
//	EXTI_InitStructure.EXTI_Trigger=EXTI_Trigger_Rising;//上升沿触发
//	EXTI_Init(&EXTI_InitStructure);//配置中断
//	
//	NVIC_InitTypeDef NVIC_InitStructure;
//	NVIC_InitStructure.NVIC_IRQChannel=RTC_Alarm_IRQn;
//	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0x02;//抢占优先级
//	NVIC_InitStructure.NVIC_IRQChannelSubPriority=0x02;//子优先级
//	NVIC_Init(&NVIC_InitStructure);//配置中断优先级
//}
 
//周期性唤醒定时器设备
//psr:预分频值，库函数配置的预分频值如下
//arr：自动重装载值,计数器CNT值减到0产生中断
//#define RTC_WakeUpClock_RTCCLK_Div16        ((uint32_t)0x00000000)
//#define RTC_WakeUpClock_RTCCLK_Div8         ((uint32_t)0x00000001)
//#define RTC_WakeUpClock_RTCCLK_Div4         ((uint32_t)0x00000002)
//#define RTC_WakeUpClock_RTCCLK_Div2         ((uint32_t)0x00000003)
//#define RTC_WakeUpClock_CK_SPRE_16bits      ((uint32_t)0x00000004)
//#define RTC_WakeUpClock_CK_SPRE_17bits      ((uint32_t)0x00000006)
void RTC_Set_WakeUp(u32 psr,u16 arr)
{
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	RTC_WakeUpCmd(DISABLE);//关闭唤醒设备WakeUp
	RTC_WakeUpClockConfig(psr);//配置时钟分频系数
	RTC_SetWakeUpCounter(arr);//设置Wake Up自动重装载寄存器

	RTC_ClearITPendingBit(RTC_IT_WUT);//清除RTC Wake Up中断标志位
	EXTI_ClearITPendingBit(EXTI_Line22);//清除LINE22上中断标志位
	
	RTC_ITConfig(RTC_IT_WUT,ENABLE);//开启RTC唤醒中断
	RTC_WakeUpCmd(ENABLE);//开启唤醒设备WakeUp
	

	EXTI_InitStructure.EXTI_Line=EXTI_Line22;
	EXTI_InitStructure.EXTI_LineCmd=ENABLE;
	EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;//中断
	EXTI_InitStructure.EXTI_Trigger=EXTI_Trigger_Rising;//上升沿触发
	EXTI_Init(&EXTI_InitStructure);//配置相应的中断
	

	NVIC_InitStructure.NVIC_IRQChannel=RTC_WKUP_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0x02;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=0x02;
	NVIC_Init(&NVIC_InitStructure);
}
 
//RTC闹钟中断
void RTC_Alarm_IRQHandler(void)
{
	if(RTC_GetFlagStatus(RTC_FLAG_ALRAF)==SET)//闹钟A中断状态位是否为1
	{
		RTC_ClearFlag(RTC_FLAG_ALRAF);//清除中断标志
		printf("helloworld!\r\n");
	}
	EXTI_ClearITPendingBit(EXTI_Line17);//清除中断线17的中断标志
}
 
//RTC WAKE UP中断服务函数
void RTC_WKUP_IRQHandler(void)
{
	if(RTC_GetFlagStatus(RTC_FLAG_WUTF)==SET)//判断WK_UP中断状态位是否为1
	{
		RTC_ClearFlag(RTC_FLAG_WUTF);//清空中断标志
		// printf("wake up %d!!\r\n", overflow_count);
        overflow_count ++;
		//LED1=!LED1;
        if (overflow_count >= target_overflows)
        {
            // 36小时到了
            overflow_count = 0;
            __set_FAULTMASK(1); //关闭总中断
            NVIC_SystemReset(); //请求单片机重启
        }
	}
	EXTI_ClearITPendingBit(EXTI_Line22);//清除中断线22的中断标志 	
}

/**
* @brief  停机唤醒后配置系统时钟: 使能 HSE, PLL
*         并且选择PLL作为系统时钟.
* @param  None
* @retval None
*/
void SYSCLKConfig_STOP(void)
{
    /* After wake-up from STOP reconfigure the system clock */
    /* 使能 HSE */
    RCC_HSEConfig(RCC_HSE_ON);

    /* 等待 HSE 准备就绪 */
    while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET);

    /* 使能 PLL */
    RCC_PLLCmd(ENABLE);

    /* 等待 PLL 准备就绪 */
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

    /* 选择PLL作为系统时钟源 */
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    /* 等待PLL被选择为系统时钟源 */
    while (RCC_GetSYSCLKSource() != 0x08);
}


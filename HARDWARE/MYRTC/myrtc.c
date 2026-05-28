#include "stm32f4xx.h"                  
#include "myrtc.h"
#include "delay.h"
#include "usart.h"
#include "LED.h"

volatile uint32_t overflow_count = 0;
const uint32_t target_overflows = 72; // 溢出次数：当 WAKE_UP_SECONDS=3600 时，每 1 小时溢出 1 次；target_overflows=2 即约 2 小时重启

static uint32_t g_lsi_freq_hz = 0;

#define MYRTC_RESET_REASON_WKUP (0xA2260001u)

static uint32_t myrtc_tim5_clock_hz(void)
{
    RCC_ClocksTypeDef clocks;
    uint32_t timclk_hz = 0;

    RCC_GetClocksFreq(&clocks);
    timclk_hz = clocks.PCLK1_Frequency;
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
    {
        timclk_hz *= 2u;
    }
    return timclk_hz;
}

static uint32_t myrtc_measure_lsi_hz(void)
{
    uint32_t capture1 = 0;
    uint32_t capture2 = 0;
    uint32_t period = 0;
    uint32_t timclk_hz = 0;
    uint32_t timeout = 0;

    TIM_TimeBaseInitTypeDef tim;
    TIM_ICInitTypeDef ic;

    RCC_LSICmd(ENABLE);
    timeout = 0;
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
    {
        timeout++;
        if (timeout > 1000u)
        {
            return 0;
        }
        delay_ms(1);
    }

    timclk_hz = myrtc_tim5_clock_hz();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
    TIM_DeInit(TIM5);

    /* LSI is internally connected to TIM5 CH4 input capture. */
    TIM_RemapConfig(TIM5, TIM5_LSI);

    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Period = 0xFFFFFFFFu;
    tim.TIM_Prescaler = 0;
    tim.TIM_ClockDivision = TIM_CKD_DIV1;
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM5, &tim);

    TIM_ICStructInit(&ic);
    ic.TIM_Channel = TIM_Channel_4;
    ic.TIM_ICPolarity = TIM_ICPolarity_Rising;
    ic.TIM_ICSelection = TIM_ICSelection_DirectTI;
    ic.TIM_ICPrescaler = TIM_ICPSC_DIV8; /* Capture every 8 cycles for better resolution. */
    ic.TIM_ICFilter = 0;
    TIM_ICInit(TIM5, &ic);

    TIM_ClearFlag(TIM5, TIM_FLAG_CC4);
    TIM_Cmd(TIM5, ENABLE);

    timeout = 0;
    while (TIM_GetFlagStatus(TIM5, TIM_FLAG_CC4) == RESET)
    {
        timeout++;
        if (timeout > 0x1FFFFFu)
        {
            TIM_Cmd(TIM5, DISABLE);
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, DISABLE);
            return 0;
        }
    }
    capture1 = TIM_GetCapture4(TIM5);
    TIM_ClearFlag(TIM5, TIM_FLAG_CC4);

    timeout = 0;
    while (TIM_GetFlagStatus(TIM5, TIM_FLAG_CC4) == RESET)
    {
        timeout++;
        if (timeout > 0x1FFFFFu)
        {
            TIM_Cmd(TIM5, DISABLE);
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, DISABLE);
            return 0;
        }
    }
    capture2 = TIM_GetCapture4(TIM5);

    TIM_Cmd(TIM5, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, DISABLE);

    if (capture2 >= capture1)
    {
        period = capture2 - capture1;
    }
    else
    {
        period = (0xFFFFFFFFu - capture1) + capture2 + 1u;
    }
    if (period == 0)
    {
        return 0;
    }

    /* TIM_ICPSC_DIV8 => captured period is 8 / f_LSI seconds. */
    return (uint32_t)(((uint64_t)timclk_hz * 8u + (period / 2u)) / (uint64_t)period);
}

static uint32_t myrtc_get_rtcck_hz(void)
{
    uint32_t rtcsel = (RCC->BDCR & RCC_BDCR_RTCSEL);

    if (rtcsel == RCC_BDCR_RTCSEL_0)
    {
        /* LSE = 32.768kHz. */
        return 32768u;
    }
    if (rtcsel == RCC_BDCR_RTCSEL_1)
    {
        /* LSI (variable). Measure once and cache. */
        if (g_lsi_freq_hz == 0)
        {
            g_lsi_freq_hz = myrtc_measure_lsi_hz();
        }
        return g_lsi_freq_hz;
    }
    if (rtcsel == (RCC_BDCR_RTCSEL_0 | RCC_BDCR_RTCSEL_1))
    {
        /* HSE divided by RTCPRE (2..31). */
        uint32_t div = (RCC->CFGR & RCC_CFGR_RTCPRE) >> 16;
        if (div < 2u)
        {
            div = 2u;
        }
        return HSE_VALUE / div;
    }

    return 0;
}

uint16_t RTC_WakeUpSecondsToArr(uint32_t seconds)
{
    uint32_t rtcck_hz = 0;
    uint32_t prediv_s = 0;
    uint32_t prediv_a = 0;
    uint64_t denom = 0;
    uint64_t ticks = 0;

    if (seconds == 0)
    {
        return 0;
    }

    rtcck_hz = myrtc_get_rtcck_hz();
    prediv_s = (RTC->PRER & 0x7FFFu);
    prediv_a = ((RTC->PRER >> 16) & 0x7Fu);
    denom = (uint64_t)(prediv_s + 1u) * (uint64_t)(prediv_a + 1u);

    if ((rtcck_hz == 0) || (denom == 0))
    {
        /* Fallback: assume CK_SPRE=1Hz. */
        if (seconds > 0x10000u)
        {
            return 0xFFFFu;
        }
        return (uint16_t)(seconds - 1u);
    }

    /* ticks = round(seconds * CK_SPRE_Hz) */
    ticks = ((uint64_t)seconds * (uint64_t)rtcck_hz + (denom / 2u)) / denom;
    if (ticks == 0)
    {
        ticks = 1;
    }
    if (ticks > 0x10000ull)
    {
        ticks = 0x10000ull;
    }
    return (uint16_t)(ticks - 1ull);
}

uint32_t RTC_GetLSIFrequencyHz(void)
{
    return g_lsi_freq_hz;
}

//时间设置函数
//hour、min、sec：小时、分钟、秒
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
//week：星期 1~7 有效，0 非法
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
 
//RTC 初始化
//返回值：0 初始化成功
//		  1 RTC 时钟源启动失败
//板子无LSE 采用LSI
u8 My_RTC_Init(void)
{
	u8 retry=0;
	u16 ck = 0;
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR,ENABLE);//使能PWR时钟
	PWR_BackupAccessCmd(ENABLE);//使能后备寄存器访问

    /* Print last reset reason marker if set by RTC_WKUP IRQ. */
    {
        uint32_t last_marker = RTC_ReadBackupRegister(RTC_BKP_DR1);
        if (last_marker != 0)
        {
            printf("RTC_BKP_DR1(last reset marker)=0x%08lx\r\n", last_marker);
            RTC_WriteBackupRegister(RTC_BKP_DR1, 0);
        }
    }

    /* If RTC already configured (backup domain kept), don't reselect clock source. */
    ck = (u16)RTC_ReadBackupRegister(RTC_BKP_DR0);
    if (ck == 0x9528)
    {
        RCC_RTCCLKCmd(ENABLE);
        RTC_WaitForSynchro();
        return 0;
    }

    /* Try LSE first; if not available, fall back to LSI. */
	RCC_LSEConfig(RCC_LSE_ON);//LSE开始，打开时钟
	delay_ms(20);
	while(RCC_GetFlagStatus(RCC_FLAG_LSERDY)==RESET)//while 循环判断 LSE 启动状态位是否置1
		//while 循环退出条件：RCC_FLAG_LSERDY 置1，表示 LSE 启动成功
	{
		retry++;
		delay_ms(10);
		if(retry>=500) break;//LSE 启动可能较慢，最多等待 5s
	}

    if (RCC_GetFlagStatus(RCC_FLAG_LSERDY) != RESET)
    {
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
    }
    else
    {
        RCC_LSEConfig(RCC_LSE_OFF);
        RCC_LSICmd(ENABLE);
        retry = 0;
        while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET)
        {
            retry++;
            delay_ms(10);
            if (retry >= 200)
            {
                return 1; /* LSI failed */
            }
        }
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    }

	RCC_RTCCLKCmd(ENABLE);//设置完 RTC 时钟以后，使能 RTC
	RTC_WaitForSynchro();
	ck = (u16)RTC_ReadBackupRegister(RTC_BKP_DR0);
//	printf("RTC_BKP_DR0 = %x\r\n",ck);
	if(ck!=0x9528)//判断是否为第一次配置：BKP_DR0 != 0x9528
		//RTC_ReadBackupRegister 用于读取后备寄存器，RTC_BKP_DR0 用作配置标志位
	//若为第一次配置则执行下述初始化；否则（后备域保持）跳过
	{

		RTC_InitTypeDef RTC_InitStructure;
		
		RTC_InitStructure.RTC_AsynchPrediv=0x7F;//异步预分频系数：7位有效 1~0x7F
		RTC_InitStructure.RTC_HourFormat=RTC_HourFormat_24; //24小时格式
		RTC_InitStructure.RTC_SynchPrediv=0xFF;//同步预分频系数：15位有效 0~0x7FFF
		
		RTC_Init(&RTC_InitStructure);  //RTC_Init 为库函数接口；本文件初始化函数命名为 My_RTC_Init，避免冲突
		
		RTC_Set_Time(15,38,50,RTC_H12_AM);//设置时间（仅第一次配置时执行）
		RTC_Set_Date(24,7,11,4);  	
		
		RTC_WriteBackupRegister(RTC_BKP_DR0,0x9528);//写入 BKP_DR0 标志位，避免下次重启重复初始化
		//断电重启后不再重复执行上述初始化；if 判断使用的标志位为 0x9528
		//如需每次重启都重新设置时间，可更改 BKP_DR0 标志位或清空后备域
	}
	return 0;//初始化成功
}
 
////设置闹钟时间
////week：星期 1~7 有效
////hour、min、sec：小时、分钟、秒
//void RTC_Set_AlarmA(u8 week,u8 hour,u8 min,u8 sec)//STM32 RTC 有 2 组可编程闹钟 A/B，这里使用闹钟 A，闹钟 B 需要额外配置
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
//	RTC_AlarmTypeInitStructure.RTC_AlarmMask=RTC_AlarmMask_None;//精确匹配到星期、时、分、秒
//	RTC_AlarmTypeInitStructure.RTC_AlarmTime=RTC_TimeTypeInitStructure;
//	RTC_SetAlarm(RTC_Format_BIN,RTC_Alarm_A,&RTC_AlarmTypeInitStructure);//设置闹钟参数
//	
//	RTC_ClearITPendingBit(RTC_IT_ALRA);//清除 RTC 闹钟A 中断标志位
//	EXTI_ClearITPendingBit(EXTI_Line17);//清除 EXTI Line17 的中断标志位
//	
//	RTC_ITConfig(RTC_IT_ALRA,ENABLE);//开启闹钟A中断
//	RTC_AlarmCmd(RTC_Alarm_A,ENABLE);//开启闹钟A
//	
//	EXTI_InitTypeDef EXTI_InitStructure;
//	EXTI_InitStructure.EXTI_Line=EXTI_Line17;//LINE17
//	EXTI_InitStructure.EXTI_LineCmd=ENABLE;//使能
//	EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;//中断模式
//	EXTI_InitStructure.EXTI_Trigger=EXTI_Trigger_Rising;//上升沿触发
//	EXTI_Init(&EXTI_InitStructure);//配置 EXTI
//	
//	NVIC_InitTypeDef NVIC_InitStructure;
//	NVIC_InitStructure.NVIC_IRQChannel=RTC_Alarm_IRQn;
//	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
//	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0x02;//抢占优先级
//	NVIC_InitStructure.NVIC_IRQChannelSubPriority=0x02;//子优先级
//	NVIC_Init(&NVIC_InitStructure);//配置 NVIC 优先级
//}
 
//周期性唤醒定时器设置
//psr：预分频选择，见 RTC_WakeUpClock_xxx
//arr：自动重装载值，计数器到 0 产生唤醒中断
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
	RTC_WakeUpCmd(DISABLE);//关闭唤醒设置 WakeUp
	RTC_WakeUpClockConfig(psr);//配置唤醒时钟分频系数
	RTC_SetWakeUpCounter(arr);//设置 WakeUp 自动重装载寄存器

	RTC_ClearITPendingBit(RTC_IT_WUT);//清除 RTC WakeUp 中断标志位
	EXTI_ClearITPendingBit(EXTI_Line22);//清除 EXTI Line22 中断标志位
	
	RTC_ITConfig(RTC_IT_WUT,ENABLE);//开启 RTC WakeUp 中断
	RTC_WakeUpCmd(ENABLE);//开启唤醒设置 WakeUp
	

	EXTI_InitStructure.EXTI_Line=EXTI_Line22;
	EXTI_InitStructure.EXTI_LineCmd=ENABLE;
	EXTI_InitStructure.EXTI_Mode=EXTI_Mode_Interrupt;//中断模式
	EXTI_InitStructure.EXTI_Trigger=EXTI_Trigger_Rising;//上升沿触发
	EXTI_Init(&EXTI_InitStructure);//配置 EXTI Line22 中断
	

	NVIC_InitStructure.NVIC_IRQChannel=RTC_WKUP_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0x02;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=0x02;
	NVIC_Init(&NVIC_InitStructure);
}
 
//RTC 闹钟中断
void RTC_Alarm_IRQHandler(void)
{
	if(RTC_GetFlagStatus(RTC_FLAG_ALRAF)==SET)//判断闹钟A中断状态位是否为1
	{
		RTC_ClearFlag(RTC_FLAG_ALRAF);//清除闹钟A标志
	}
	EXTI_ClearITPendingBit(EXTI_Line17);//清除 EXTI Line17 中断标志
}
 
//RTC WakeUp 中断服务函数
void RTC_WKUP_IRQHandler(void)
{
	if(RTC_GetITStatus(RTC_IT_WUT) == SET)//判断 WakeUp 中断状态位是否为1
	{
		RTC_ClearITPendingBit(RTC_IT_WUT);//清空 WakeUp 中断标志
        overflow_count ++;
		//LED1=!LED1;
        if (overflow_count >= target_overflows)
        {
            /* 到达设定重启周期 */
            overflow_count = 0;
            RTC_WriteBackupRegister(RTC_BKP_DR1, MYRTC_RESET_REASON_WKUP);
            NVIC_SystemReset(); //请求单片机重启
        }
	}
	EXTI_ClearITPendingBit(EXTI_Line22);//清除 EXTI Line22 中断标志
}

/**
* @brief  STOP 模式唤醒后重新配置系统时钟: 使能 HSE, PLL
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

    /* 选择PLL作为系统时钟 */
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    /* 等待PLL被选择为系统时钟源 */
    while (RCC_GetSYSCLKSource() != 0x08);
}


#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "beep.h"
#include "key.h"
#include "Mb_usart.h"
#include "crc16.h"
#include "mdbs_func.h"
#include "ec20.h"
#include "timer.h"
#include "FreeRTOS.h"
#include "task.h"
#include "freertos_demo.h"
#include "mac.h"
#include "tree_node.h"
#include "routing.h"
#include "iwdg.h"
#include "ewdg.h"
#include "myrtc.h"
#include "ina226.h"
#include "adaptive_report.h"

int main(void)
{ 	
		NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);//设置系统中断优先级分组4
		delay_init(168);		//延时初始化 
		delay_xms(50);//加这个延时，防止外设还没来得及上电的时候就已经开始初始化了
		EWDG_Init();
		TIM2_Init((u16)83999, (u16)299); // PSC=83999, ARR=299，定时器2每300ms中断一次来喂狗
		uart_init(115200);	//串口初始化波特率为115200

        /* 打印复位原因标志位（用于区分RTC软件复位、看门狗复位等） */
        {
            uint32_t csr = RCC->CSR;
            printf("RCC_CSR=0x%08lx\r\n", csr);
            if (csr & RCC_CSR_SFTRSTF)  printf("ResetFlag: SFTRSTF (software)\r\n");
#if defined(RCC_CSR_WDGRSTF)
            if (csr & RCC_CSR_WDGRSTF)  printf("ResetFlag: WDGRSTF (IWDG)\r\n");
#elif defined(RCC_CSR_IWDGRSTF)
            if (csr & RCC_CSR_IWDGRSTF) printf("ResetFlag: IWDGRSTF (IWDG)\r\n");
#endif
            if (csr & RCC_CSR_WWDGRSTF) printf("ResetFlag: WWDGRSTF\r\n");
            if (csr & RCC_CSR_BORRSTF)  printf("ResetFlag: BORRSTF\r\n");
            if (csr & RCC_CSR_PORRSTF)  printf("ResetFlag: PORRSTF\r\n");
#if defined(RCC_CSR_PADRSTF)
            if (csr & RCC_CSR_PADRSTF)  printf("ResetFlag: PADRSTF\r\n");
#elif defined(RCC_CSR_PINRSTF)
            if (csr & RCC_CSR_PINRSTF)  printf("ResetFlag: PINRSTF\r\n");
#endif
            if (csr & RCC_CSR_LPWRRSTF) printf("ResetFlag: LPWRRSTF\r\n");
            RCC->CSR |= RCC_CSR_RMVF; /* 清除复位标志位 */

        }

		Mdbus_CTRL_Init();
		Usart3_init(4800);
		PWR_sensor_CTRL();
		LED_Init();
		KEY_Init();
		INA226_Init(INA226_I2C_ADDR_DEFAULT);
		AdaptiveReport_Init();

		SensorDataGet(); //测试传感器数据

#if IS_GATWAY	
		EC800_Init();
		MqttConnect();
#endif
		lora_init(0x31415926);
		IWDG_Init(IWDG_Prescaler_256,2000);//时间计算(大概):Tout=256 * rlr/32 (ms) = 8*rlr(ms) rlr取值范围0-2047
		if(My_RTC_Init() == 0)
        {
            u16 wut_arr = (u16)RTC_WakeUpSecondsToArr(WAKE_UP_SECONDS);
            printf("RTC BDCR=0x%08lx, LSI=%luHz, WAKE_UP_SECONDS=%lu, WUTR=%u\r\n",
                   RCC->BDCR,
                   (unsigned long)RTC_GetLSIFrequencyHz(),
                   (unsigned long)WAKE_UP_SECONDS,
                   (unsigned int)wut_arr);
            RTC_Set_WakeUp(RTC_WakeUpClock_CK_SPRE_16bits, wut_arr);
        }
        else
        {
            printf("My_RTC_Init failed\r\n");
        }
		TIM_Cmd(TIM2, DISABLE);//关闭TIM2，在FreeRTOS中喂狗
		freertos_demo();
}
 

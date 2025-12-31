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


int main(void)
{ 	

		NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);//设置系统中断优先级分组4
		delay_init(168);		//延时初始化 
		delay_xms(50);//加这个延时，防止外设还没来得及上电的时候就已经开始初始化了
		EWDG_Init();
		TIM2_Init((u16)83999, (u16)299); // PSC=83999, ARR=299，定时器2每300ms中断一次来喂狗
		uart_init(115200);	//串口初始化波特率为115200
		Mdbus_CTRL_Init();
		Usart3_init(4800);
    	PWR_sensor_CTRL();
		LED_Init();
		KEY_Init();
		
		SensorDataGet(); //测试传感器数据
#if IS_GATWAY	
    	EC800_Init();
    	MqttConnect();
#endif
		lora_init(0x31415926);
		IWDG_Init(IWDG_Prescaler_256,2000);//时间计算(大概):Tout=256 * rlr/32 (ms) = 8*rlr(ms) rlr取值范围0-2047
		My_RTC_Init();
		RTC_Set_WakeUp(RTC_WakeUpClock_CK_SPRE_16bits, WAKE_UP_SECONDS - 1); //配置WAKE UP中断，WAKE_UP_SECONDS秒钟中断一次
		TIM_Cmd(TIM2, DISABLE);//关闭TIM2，在FreeRTOS中喂狗
		freertos_demo();
}
 

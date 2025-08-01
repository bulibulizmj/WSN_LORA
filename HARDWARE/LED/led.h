#ifndef __LED_H
#define __LED_H
#include "sys.h"

//三个LED灯定义
#define LED0 PFout(9)	// DS0
#define LED1 PFout(10)	// DS1

#define PWR_I2C PFout(4)//低电平关闭供电，高电平开启供电
#define PWR_Mdbs PGout(8)//低电平关闭供电，高电平开启供电
#define PWR_LORA PDout(6)//低电平关闭供电，高电平开启供电

void LED_Init(void);//初始化
void Play_LED(void);
void PWR_sensor_CTRL(void);
void sensor_power_off(void);
void sensor_power_on(void);


#endif

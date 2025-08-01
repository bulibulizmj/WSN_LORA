#include "led.h" 	    
#include "delay.h"
//LED IO初始化
void LED_Init(void)
{    	 
    GPIO_InitTypeDef  GPIO_InitStructure;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE); //初始化GPIOG


    //三路LED PC4 PA7 PC0
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
//    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
//    GPIO_Init(GPIOF,&GPIO_InitStructure);
//    GPIO_ResetBits(GPIOF,GPIO_Pin_9);//LED2

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_Init(GPIOF,&GPIO_InitStructure);
    GPIO_SetBits(GPIOF,GPIO_Pin_9);//LED0
    GPIO_SetBits(GPIOF,GPIO_Pin_10);//LED1


}

void Play_LED(void)
{
    delay_ms(1000);
    GPIO_SetBits(GPIOF,GPIO_Pin_9);//LED0
    delay_ms(1000);
    GPIO_SetBits(GPIOF,GPIO_Pin_10);//LED1
}

void PWR_sensor_CTRL(void)
{	
		GPIO_InitTypeDef GPIO_InitStructure;

		/*开启GPIO时钟*/	
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);
		//定义一个GPIO初始化结构体

		//配置GPIO初始化结构体的成员
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
		//调用GPIO初始化函数，把配置好的结构体成员的参数写入寄存器

		GPIO_Init(GPIOD, &GPIO_InitStructure);
		
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
		GPIO_Init(GPIOF, &GPIO_InitStructure);

		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
		GPIO_Init(GPIOG, &GPIO_InitStructure);
		
		GPIO_SetBits(GPIOD,GPIO_Pin_6);
		GPIO_SetBits(GPIOG,GPIO_Pin_8);
		GPIO_SetBits(GPIOF,GPIO_Pin_4);
}

void sensor_power_off()
{
		PWR_I2C = 1;
		PWR_Mdbs = 0;
		PWR_LORA = 1;
}


void sensor_power_on()
{
    PWR_I2C = 1;
		PWR_Mdbs = 1;
		PWR_LORA = 1;
}





#ifndef __EWDG_H
#define __EWDG_H

#include "stm32f4xx.h"

// 引脚定义
#define WDI_GPIO_PORT     GPIOF
#define WDI_GPIO_PIN      GPIO_Pin_8
#define WDI_GPIO_CLK      RCC_AHB1Periph_GPIOF

// 函数声明
void EWDG_Init(void);           // 初始化喂狗引脚
void EWDG_Feed(void);           // 喂狗函数（翻转电平）

#endif


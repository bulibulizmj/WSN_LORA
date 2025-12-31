#include "ewdg.h"

// 计数器，用于计时喂狗间隔
static __IO uint32_t wdg_counter = 0;

// 初始化PF8为推挽输出
void EWDG_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 开启GPIOF时钟
    RCC_AHB1PeriphClockCmd(WDI_GPIO_CLK, ENABLE);
    
    // 配置PF8
    GPIO_InitStructure.GPIO_Pin = WDI_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;      // 输出模式
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;     // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;   // 低速即可
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;   // 不上拉不下拉
    GPIO_Init(WDI_GPIO_PORT, &GPIO_InitStructure);
    
    // 初始化为低电平
    GPIO_ResetBits(WDI_GPIO_PORT, WDI_GPIO_PIN);
}

// 喂狗函数：翻转PF8电平
void EWDG_Feed(void)
{
    GPIO_ToggleBits(WDI_GPIO_PORT, WDI_GPIO_PIN);
}


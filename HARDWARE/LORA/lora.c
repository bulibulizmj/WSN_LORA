
#include "lora.h"
#include "stdlib.h"
#include "string.h"


int errcount_LORA = 0;

char *strx;
extern u8 lora_serialRXbuf_st[LORA_SERIAL_BUF_SIZE];
extern u16 lora_Rxcouter;

void Clear_Buffer_LORA(void)//清空缓存
{
    u16 i;
    for(i=0;i<lora_Rxcouter;i++)
        lora_serialRXbuf_st[i]=0;//缓存
    lora_Rxcouter=0;
    //	IWDG_Feed();//喂狗
}





/**
 * @brief       ATK-MW1268D模块硬件初始化,AUX引脚在接收时可唤醒设备, 作为中继站的时候，不需要AUX引脚可唤醒设备，需要4g模块可唤醒设备，作为发送站的时候，需要AUX引脚连接外部中断线，取消该函数以及中断服务函数注释即可
 * @param       无
 * @retval      无
 */
static void atk_mw1268d_hw_init(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;
//		EXTI_InitTypeDef EXTI_InitStructure;
		

		RCC_APB2PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
		
		/*开启GPIO时钟*/	
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);
		//定义一个GPIO初始化结构体

		//配置GPIO初始化结构体的成员
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;//MD0
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
		//调用GPIO初始化函数，把配置好的结构体成员的参数写入寄存器

		GPIO_Init(GPIOG, &GPIO_InitStructure);




		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;	    		 //AUX
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;//普通输入模式
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100M
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
		GPIO_Init(GPIOA, &GPIO_InitStructure);	  				 //推挽输出 ，IO口速度为50MHz  
		ATK_MW1268D_MD0_GPIO_PIN = 1;//MD0输出低
	
//		GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource6);

//	
//		EXTI_InitStructure.EXTI_Line = EXTI_Line6;
//		EXTI_InitStructure.EXTI_LineCmd = ENABLE;
//		EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
//		EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
//		EXTI_Init(&EXTI_InitStructure);
//		
//		NVIC_InitTypeDef NVIC_InitStructure;
//		NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
//		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
//		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
//		NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
//		NVIC_Init(&NVIC_InitStructure);
}

/**
 * @brief       调用ATK-MW1268D模块初始化函数，若失败50次则重启
 * @param       
 * @retval      
 *              
 */
void lora_init(u32 secret)
{
		u16 errorcount = 0;
		while(atk_mw1268d_init(115200, secret) != ATK_MW1268D_EOK)
		{
				errorcount++;
				if(errorcount >50)
				{
						__set_FAULTMASK(1);
						NVIC_SystemReset();	//超时重启
						break;
				}
		}
}


/**
 * @brief       ATK-MW1268D模块初始化
 * @param       baudrate: ATK-MW1268D模块UART通讯波特率
 * @retval      ATK_MW1268D_EOK  : ATK-MW1268D模块初始化成功，函数执行成功
 *              ATK_MW1268D_ERROR: ATK-MW1268D模块初始化失败，函数执行失败
 */
uint8_t atk_mw1268d_init(uint32_t baudrate, u32 secret)
{
		uint8_t ret;
    
    atk_mw1268d_hw_init();                          /* 硬件初始化 */
    uart4_init(baudrate);                /* UART初始化 */
    atk_mw1268d_enter_config();                     /* 进入配置模式 */
		delay_ms(20);
    ret = atk_mw1268d_at_test();                    /* AT指令测试 */
    ret  = atk_mw1268d_addr_config(0);
    ret += atk_mw1268d_netid_config(0);
	  ret += atk_mw1268d_tpower_config(ATK_MW1268D_TPOWER_30DBM);
    ret += atk_mw1268d_wlrate_channel_config(ATK_MW1268D_WLRATE_0K3, 23);
    ret += atk_mw1268d_workmode_config(ATK_MW1268D_WORKMODE_NORMAL);
    ret += atk_mw1268d_tmode_config(ATK_MW1268D_TMODE_TT);
    ret += atk_mw1268d_packsize_config(ATK_MW1268D_PACKSIZE_240);
    ret += atk_mw1268d_wltime_config(ATK_MW1268D_WLTIME_1S);
    ret += atk_mw1268d_uart_config(ATK_MW1268D_UARTRATE_115200BPS, ATK_MW1268D_UARTPARI_NONE);
    ret += atk_mw1268d_lbt_config(ATK_MW1268D_ENABLE);
    ret += atk_mw1268d_datakey_config(secret);    
    ret += atk_mw1268d_rssi_config(ATK_MW1268D_ENABLE);   
	
		atk_mw1268d_exit_config();                      /* 退出配置模式 */
    if ((ret != ATK_MW1268D_EOK)&&(ret != 0x0a))
    {
				printf("lora config failed\r\n");
        return ATK_MW1268D_ERROR;
    }
    else
		{
				printf("lora cofig success\r\n");
				return ATK_MW1268D_EOK;
		}
}

/**
 * @brief       修改ATK-MW1268D模块秘钥
 * @param       secret: 秘钥
 * @retval      无
 */
void atk_mw1268d_change_secret(u32 secret)
{
//		atk_mw1268d_hw_init();                          /* 硬件初始化 */
//    uart4_init(115200);               							/* UART初始化 */
		atk_mw1268d_enter_config();                     /* 进入配置模式 */
		UART4->SR |= 0X40;
		delay_ms(500);
//    atk_mw1268d_at_test();                    			/* AT指令测试 */
		atk_mw1268d_datakey_config(secret);   
	
	
		atk_mw1268d_exit_config(); 
}

/**
 * @brief       修改ATK-MW1268D模块秘钥
 * @param       mode: 模式
 * @retval      无
 */
void atk_mw1268d_change_mode(atk_mw1268d_workmode_t mode)
{
  char cmd[50] = {0};
//		atk_mw1268d_hw_init();                          /* 硬件初始化 */
//    uart4_init(115200);               							/* UART初始化 */
		atk_mw1268d_enter_config();                       /* 进入配置模式 */
		UART4->SR |= 0X40;
		delay_xms(1000);
//    atk_mw1268d_at_test();                    			/* AT指令测试 */
    sprintf(cmd, "AT+CWMODE=%d\r\n", mode);
		Uart4_SendStr(cmd);
    delay_xms(1000);
		atk_mw1268d_exit_config(); 
}

/**
 * @brief       ATK-MW1268D模块进入配置模式
 * @param       无
 * @retval      无
 */
void atk_mw1268d_enter_config(void)
{
    g_lora_rx_framing_enable = 0;
    ATK_MW1268D_MD0_GPIO_PIN =1;
}


/**
 * @brief       ATK-MW1268D模块退出配置模式
 * @param       无
 * @retval      无
 */
void atk_mw1268d_exit_config(void)
{
    g_lora_rx_framing_enable = 1;
    ATK_MW1268D_MD0_GPIO_PIN = 0;
}


/**
 * @brief       判断ATK-MW1268D模块是否空闲
 * @note        仅当ATK-MW1268D模块空闲的时候，才能发送数据
 * @param       无
 * @retval      ATK_MW1268D_EOK  : ATK-MW1268D模块空闲
 *              ATK_MW1268D_EBUSY: ATK-MW1268D模块忙
 */
uint8_t atk_mw1268d_free(void)
{
    if (ATK_MW1268D_AUX_GPIO_PIN != 0)
    {
        return ATK_MW1268D_EBUSY;
    }
    
    return ATK_MW1268D_EOK;
}


/**
 * @brief       ATK-MW1268D模块AT指令测试
 * @param       无
 * @retval      ATK_MW1268D_EOK  : AT指令测试成功
 *              ATK_MW1268D_ERROR: AT指令测试失败
 */
uint8_t atk_mw1268d_at_test(void)
{
    Clear_Buffer_LORA();
    Uart4_SendStr("AT\r\n");
    delay_ms(1000);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    errcount_LORA = 0;
		while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新连接模块...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr("AT\r\n");
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>50)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
				delay_ms(1);
		}
		Clear_Buffer_LORA();
		Uart4_SendStr("ATE1\r\n"); //回显
    delay_ms(500);
		Clear_Buffer_LORA();	

		return ATK_MW1268D_EOK;   
}

/**
 * @brief       ATK-MW1268D模块设备地址配置
 * @param       addr: 设备地址 0-255
 * @retval      ATK_MW1268D_EOK   : 设备地址配置成功
 *              ATK_MW1268D_ERROR : 设备地址配置失败
 */
uint8_t atk_mw1268d_addr_config(uint16_t addr)
{
    char cmd[50] = {0};
    
    sprintf(cmd, "AT+ADDR=%02X,%02X\r\n", (uint8_t)(addr >> 8) & 0xFF, (uint8_t)addr & 0xFF);
    Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置地址...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>89)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}


/**
 * @brief       ATK-MW1268D模块发射功率配置
 * @param       tpower: ATK_MW1268D_TPOWER_21DBM: 21dBm
 *                      ATK_MW1268D_TPOWER_24DBM: 24dBm
 *                      ATK_MW1268D_TPOWER_27DBM: 27dBm
 *                      ATK_MW1268D_TPOWER_30DBM: 30dBm

 * @retval      ATK_MW1268D_EOK   : 发射功率配置成功
 *              ATK_MW1268D_ERROR : 发射功率配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_tpower_config(atk_mw1268d_tpower_t tpower)
{
    char cmd[50] = {0};
    
    switch (tpower)
    {
        case ATK_MW1268D_TPOWER_21DBM:
        case ATK_MW1268D_TPOWER_24DBM:
        case ATK_MW1268D_TPOWER_27DBM:
        case ATK_MW1268D_TPOWER_30DBM:
        {
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    sprintf(cmd, "AT+TPOWER=%d\r\n", tpower);
		
    Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置发射功率...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>130)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}

/**
 * @brief       ATK-MW1268D模块工作模式配置
 * @param       workmode: ATK_MW1268D_WORKMODE_NORMAL  : 一般模式（默认）
 *                        ATK_MW1268D_WORKMODE_WAKEUP  : 唤醒模式
 *                        ATK_MW1268D_WORKMODE_LOWPOWER: 省电模式
 *                        ATK_MW1268D_WORKMODE_SIGNAL  : 信号强度模式
 *                        ATK_MW1268D_WORKMODE_SLEEP   : 睡眠模式
 *                        ATK_MW1268D_WORKMODE_RELAY   : 中继模式
 * @retval      ATK_MW1268D_EOK   : 工作模式配置成功
 *              ATK_MW1268D_ERROR : 工作模式配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_workmode_config(atk_mw1268d_workmode_t workmode)
{
    char cmd[50] = {0};
    
    switch (workmode)
    {
        case ATK_MW1268D_WORKMODE_NORMAL:
        case ATK_MW1268D_WORKMODE_WAKEUP:
        case ATK_MW1268D_WORKMODE_LOWPOWER:
        case ATK_MW1268D_WORKMODE_SIGNAL:
        case ATK_MW1268D_WORKMODE_SLEEP:
        case ATK_MW1268D_WORKMODE_RELAY:
        {
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    sprintf(cmd, "AT+CWMODE=%d\r\n", workmode);
		
		Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置工作模式...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>165)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}

/**
 * @brief       ATK-MW1268D模块发送模式配置
 * @param       tmode: ATK_MW1268D_TMODE_TT: 透明传输（默认）
 *                     ATK_MW1268D_TMODE_DT: 定向传输
 * @retval      ATK_MW1268D_EOK   : 发送模式配置成功
 *              ATK_MW1268D_ERROR : 发送模式配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_tmode_config(atk_mw1268d_tmode_t tmode)
{
    char cmd[50] = {0};
    
    switch (tmode)
    {
        case ATK_MW1268D_TMODE_TT:
        case ATK_MW1268D_TMODE_DT:
        {
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    sprintf(cmd, "AT+TMODE=%d\r\n", tmode);
    
		Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置发送模式...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>165)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;

}

/**
 * @brief       ATK-MW1268D模块空中速率和信道配置
 * @param       wlrate : ATK_MW1268D_WLRATE_0K3 : 0.3Kbps
 *                       ATK_MW1268D_WLRATE_1K2 : 1.2Kbps
 *                       ATK_MW1268D_WLRATE_2K4 : 2.4Kbps
 *                       ATK_MW1268D_WLRATE_4K8 : 4.8Kbps
 *                       ATK_MW1268D_WLRATE_9K6 : 9.6Kbps
 *                       ATK_MW1268D_WLRATE_19K2: 19.2Kbps（默认）
 *                       ATK_MW1268D_WLRATE_38K4: 38.4Kbps
 *                       ATK_MW1268D_WLRATE_62K5: 62.5Kbps
 *              channel: 信道，范围0~83
 * @retval      ATK_MW1268D_EOK   : 空中速率和信道配置成功
 *              ATK_MW1268D_ERROR : 空中速率和信道配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_wlrate_channel_config(atk_mw1268d_wlrate_t wlrate, uint8_t channel)
{
    char cmd[50] = {0};
    
    switch (wlrate)
    {
        case ATK_MW1268D_WLRATE_0K3:
        case ATK_MW1268D_WLRATE_1K2:
        case ATK_MW1268D_WLRATE_2K4:
        case ATK_MW1268D_WLRATE_4K8:
        case ATK_MW1268D_WLRATE_9K6:
        case ATK_MW1268D_WLRATE_19K2:
        case ATK_MW1268D_WLRATE_38K4:
        case ATK_MW1268D_WLRATE_62K5:
        {
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    if (channel > 83)
    {
        return ATK_MW1268D_EINVAL;
    }
    
    sprintf(cmd, "AT+WLRATE=%d,%d\r\n", channel, wlrate);
    
		Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置空中速率和信道...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>165)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}

/**
 * @brief       ATK-MW1268D模块网络地址配置
 * @param       netid: 网络地址，范围（0~255）
 * @retval      ATK_MW1268D_EOK   : 网络地址配置成功
 *              ATK_MW1268D_ERROR : 网络地址配置失败
 */
uint8_t atk_mw1268d_netid_config(uint8_t netid)
{
    char cmd[50] = {0};
    
    sprintf(cmd, "AT+NETID=%d\r\n", netid);
    
    Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置网络地址...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>165)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}

/**
 * @brief       ATK-MW1268D模块休眠时间配置
 * @param       wltime: ATK_MW1268D_WLTIME_1S: 1秒（默认）
 *                      ATK_MW1268D_WLTIME_2S: 2秒
 * @retval      ATK_MW1268D_EOK   : 休眠时间配置成功
 *              ATK_MW1268D_ERROR : 休眠时间配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_wltime_config(atk_mw1268d_wltime_t wltime)
{
    char cmd[50] = {0};
    
    switch (wltime)
    {
        case ATK_MW1268D_WLTIME_1S:
        case ATK_MW1268D_WLTIME_2S:
        {
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    sprintf(cmd, "AT+WLTIME=%d\r\n", wltime);
    
    Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置休眠时间...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>195)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}

/**
 * @brief       ATK-MW1268D模块数据包大小配置
 * @param       packsize: ATK_MW1268D_PACKSIZE_32 : 32字节
 *                        ATK_MW1268D_PACKSIZE_64 : 64字节
 *                        ATK_MW1268D_PACKSIZE_128: 128字节
 *                        ATK_MW1268D_PACKSIZE_240: 240字节
 * @retval      ATK_MW1268D_EOK   : 数据包大小配置成功
 *              ATK_MW1268D_ERROR : 数据包大小配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_packsize_config(atk_mw1268d_packsize_t packsize)
{
    char cmd[50] = {0};
    
    switch (packsize)
    {
        case ATK_MW1268D_PACKSIZE_32:
        case ATK_MW1268D_PACKSIZE_64:
        case ATK_MW1268D_PACKSIZE_128:
        case ATK_MW1268D_PACKSIZE_240:
        {
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    sprintf(cmd, "AT+PACKSIZE=%d\r\n", packsize);
    
    Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置数据包大小...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>220)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}

/**
 * @brief       ATK-MW1268D模块数据加密密钥配置
 * @param       datakey: 数据加密密钥，范围0~0xFFFFFFFF
 * @retval      ATK_MW1268D_EOK   : 数据包大小配置成功
 *              ATK_MW1268D_ERROR : 数据包大小配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_datakey_config(uint32_t datakey)
{
    char cmd[50] = {0};
    
    sprintf(cmd, "AT+DATAKEY=%08X\r\n", datakey);
    
    Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置秘钥...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>250)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}

/**
 * @brief       ATK-MW1268D模块串口配置
 * @param       baudrate: ATK_MW1268D_UARTRATE_1200BPS  : 1200bps
 *                        ATK_MW1268D_UARTRATE_2400BPS  : 2400bps
 *                        ATK_MW1268D_UARTRATE_4800BPS  : 4800bps
 *                        ATK_MW1268D_UARTRATE_9600BPS  : 9600bps
 *                        ATK_MW1268D_UARTRATE_19200BPS : 19200bps
 *                        ATK_MW1268D_UARTRATE_38400BPS : 38400bps
 *                        ATK_MW1268D_UARTRATE_57600BPS : 57600bps
 *                        ATK_MW1268D_UARTRATE_115200BPS: 115200bps（默认）
 *              parity  : ATK_MW1268D_UARTPARI_NONE: 无校验（默认）
 *                        ATK_MW1268D_UARTPARI_EVEN: 偶校验
 *                        ATK_MW1268D_UARTPARI_ODD : 奇校验
 * @retval      ATK_MW1268D_EOK   : 串口配置成功
 *              ATK_MW1268D_ERROR : 串口配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_uart_config(atk_mw1268d_uartrate_t baudrate, atk_mw1268d_uartpari_t parity)
{
    char cmd[50] = {0};
    
    switch (baudrate)
    {
        case ATK_MW1268D_UARTRATE_1200BPS:
        case ATK_MW1268D_UARTRATE_2400BPS:
        case ATK_MW1268D_UARTRATE_4800BPS:
        case ATK_MW1268D_UARTRATE_9600BPS:
        case ATK_MW1268D_UARTRATE_19200BPS:
        case ATK_MW1268D_UARTRATE_38400BPS:
        case ATK_MW1268D_UARTRATE_57600BPS:
        case ATK_MW1268D_UARTRATE_115200BPS:
        {
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    switch (parity)
    {
        case ATK_MW1268D_UARTPARI_NONE:
        case ATK_MW1268D_UARTPARI_EVEN:
        case ATK_MW1268D_UARTPARI_ODD:
        {
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    sprintf(cmd, "AT+UART=%d,%d\r\n", baudrate, parity);
    
    Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置模块串口...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>280)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}


/**
 * @brief       ATK-MW1268D模块信道检测配置
 * @param       enable: ATK_MW1268D_DISABLE: 关闭信道检测
 *                      ATK_MW1268D_ENABLE : 打开信道检测
 * @retval      ATK_MW1268D_EOK   : 信道检测配置成功
 *              ATK_MW1268D_ERROR : 信道检测配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_lbt_config(atk_mw1268d_enable_t enable)
{
    char cmd[50] = {0};
    
    switch (enable)
    {
        case ATK_MW1268D_DISABLE:
        {
            sprintf(cmd, "AT+LBT=0\r\n");
            break;
        }
        case ATK_MW1268D_ENABLE:
        {
            sprintf(cmd, "AT+LBT=1\r\n");
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置模块信道检测...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>300)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}

/**
 * @brief       ATK-MW1268D模块参数保存配置
 * @param       enable: ATK_MW1268D_DISABLE: 不保存参数
 *                      ATK_MW1268D_ENABLE : 保存参数
 * @retval      ATK_MW1268D_EOK   : 参数保存配置成功
 *              ATK_MW1268D_ERROR : 参数保存配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_flash_config(atk_mw1268d_enable_t enable)
{
    char cmd[50] = {0};
    
    switch (enable)
    {
        case ATK_MW1268D_DISABLE:
        {
            sprintf(cmd, "AT+FLASH=0\r\n");
            break;
        }
        case ATK_MW1268D_ENABLE:
        {
            sprintf(cmd, "AT+FLASH=1\r\n");
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置模块信道检测...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>330)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}

/**
 * @brief       ATK-MW1268D模块开启RSSI设置，开启后收到的第一个字节为RSSI强度
 * @param       enable: ATK_MW1268D_DISABLE: 不开启RSSI监测
 *                      ATK_MW1268D_ENABLE : 开启RSSI监测
 * @retval      ATK_MW1268D_EOK   : 参数保存配置成功
 *              ATK_MW1268D_ERROR : 参数保存配置失败
 *              ATK_MW1268D_EINVAL: 输入参数有误
 */
uint8_t atk_mw1268d_rssi_config(atk_mw1268d_enable_t enable)
{
    char cmd[50] = {0};
    
    switch (enable)
    {
        case ATK_MW1268D_DISABLE:
        {
            sprintf(cmd, "AT+RSSI=0\r\n");
            break;
        }
        case ATK_MW1268D_ENABLE:
        {
            sprintf(cmd, "AT+RSSI=1\r\n");
            break;
        }
        default:
        {
            return ATK_MW1268D_EINVAL;
        }
    }
    
    Uart4_SendStr(cmd);
    delay_ms(500);
		strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
    while (strx==NULL)
		{
				errcount_LORA++;
				printf("\r\n单片机正在重新配置模块信道检测...\r\n");
				Clear_Buffer_LORA();
				Uart4_SendStr(cmd);
				delay_ms(500);
				strx=strstr((const char*)lora_serialRXbuf_st,(const char*)"OK");//返回OK
				if(errcount_LORA>500)     //防止死循环
				{
						errcount_LORA = 0;
//								reset_4g();
//								__set_FAULTMASK(1); //关闭总中断
//								NVIC_SystemReset(); //请求单片机重启
						return ATK_MW1268D_ETIMEOUT;
				}
		}
		Clear_Buffer_LORA();		   
    return ATK_MW1268D_EOK;
}

//void EXTI9_5_IRQHandler(void)
//{
//	if (EXTI_GetITStatus(EXTI_Line6) == SET)
//	{
//		EXTI_ClearITPendingBit(EXTI_Line6);
//		//printf("aux change!\r\n");
//	}
//}




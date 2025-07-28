#ifndef __LORA_H
#define __LORA_H	 
#include "sys.h"
#include "stdio.h"
#include "delay.h"
#include "usart.h"
#include "mac.h"
//////////////////////////////////////////////////////////////////////////////////	 
	  
#define ATK_MW1268D_MD0_GPIO_PIN PGout(6)
#define ATK_MW1268D_AUX_GPIO_PIN PAin(12)

/* AT响应等待超时时间（毫秒） */
#define ATK_MW1268D_AT_TIMEOUT  500

/* 使能枚举 */
typedef enum
{
    ATK_MW1268D_DISABLE             = 0x00,
    ATK_MW1268D_ENABLE,
} atk_mw1268d_enable_t;

/* 发射功率枚举 */
typedef enum
{
    ATK_MW1268D_TPOWER_21DBM        = 0,   /* 9dBm */
    ATK_MW1268D_TPOWER_24DBM        = 1,   /* 11dBm */
    ATK_MW1268D_TPOWER_27DBM        = 2,   /* 14dBm */
    ATK_MW1268D_TPOWER_30DBM        = 3,   /* 17dBm */
} atk_mw1268d_tpower_t;

/* 工作模式枚举 */
typedef enum
{
    ATK_MW1268D_WORKMODE_NORMAL     = 0,    /* 一般模式（默认） */
    ATK_MW1268D_WORKMODE_WAKEUP     = 1,    /* 唤醒模式 */
    ATK_MW1268D_WORKMODE_LOWPOWER   = 2,    /* 省电模式 */
    ATK_MW1268D_WORKMODE_SIGNAL     = 3,    /* 信号强度模式 */
    ATK_MW1268D_WORKMODE_SLEEP      = 4,    /* 睡眠模式 */
    ATK_MW1268D_WORKMODE_RELAY      = 5,    /* 中继模式 */
} atk_mw1268d_workmode_t;

/* 发射模式枚举 */
typedef enum
{
    ATK_MW1268D_TMODE_TT            = 0,    /* 透明传输（默认） */
    ATK_MW1268D_TMODE_DT            = 1,    /* 定向传输 */
} atk_mw1268d_tmode_t;

/* 空中速率枚举 */
typedef enum
{
    ATK_MW1268D_WLRATE_0K3          = 0,    /* 0.3Kbps */
    ATK_MW1268D_WLRATE_1K2          = 1,    /* 1.2Kbps */
    ATK_MW1268D_WLRATE_2K4          = 2,    /* 2.4Kbps */
    ATK_MW1268D_WLRATE_4K8          = 3,    /* 4.8Kbps */
    ATK_MW1268D_WLRATE_9K6          = 4,    /* 9.6Kbps */
    ATK_MW1268D_WLRATE_19K2         = 5,    /* 19.2Kbps（默认） */
    ATK_MW1268D_WLRATE_38K4         = 6,    /* 38.4Kbps */
    ATK_MW1268D_WLRATE_62K5         = 7,    /* 62.5Kbps */
} atk_mw1268d_wlrate_t;

/* 休眠时间枚举 */
typedef enum
{
    ATK_MW1268D_WLTIME_1S           = 0,    /* 1秒（默认） */
    ATK_MW1268D_WLTIME_2S           = 1,    /* 2秒 */
} atk_mw1268d_wltime_t;

/* 数据包大小枚举 */
typedef enum
{
    ATK_MW1268D_PACKSIZE_32         = 0,    /* 32字节 */
    ATK_MW1268D_PACKSIZE_64         = 1,    /* 64字节 */
    ATK_MW1268D_PACKSIZE_128        = 2,    /* 128字节 */
    ATK_MW1268D_PACKSIZE_240        = 3,    /* 240字节 */
} atk_mw1268d_packsize_t;

/* 串口通信波特率枚举 */
typedef enum
{
    ATK_MW1268D_UARTRATE_1200BPS    = 0,    /* 1200bps */
    ATK_MW1268D_UARTRATE_2400BPS    = 1,    /* 2400bps */
    ATK_MW1268D_UARTRATE_4800BPS    = 2,    /* 4800bps */
    ATK_MW1268D_UARTRATE_9600BPS    = 3,    /* 9600bps */
    ATK_MW1268D_UARTRATE_19200BPS   = 4,    /* 19200bps */
    ATK_MW1268D_UARTRATE_38400BPS   = 5,    /* 38400bps */
    ATK_MW1268D_UARTRATE_57600BPS   = 6,    /* 57600bps */
    ATK_MW1268D_UARTRATE_115200BPS  = 7,    /* 115200bps（默认） */
} atk_mw1268d_uartrate_t;

/* 串口通讯校验位枚举 */
typedef enum
{
    ATK_MW1268D_UARTPARI_NONE       = 0,    /* 无校验（默认） */
    ATK_MW1268D_UARTPARI_EVEN       = 1,    /* 偶校验 */
    ATK_MW1268D_UARTPARI_ODD        = 2,    /* 奇校验 */
} atk_mw1268d_uartpari_t;

/* 错误代码 */
#define ATK_MW1268D_EOK             0       /* 没有错误 */
#define ATK_MW1268D_ERROR           1       /* 通用错误 */
#define ATK_MW1268D_ETIMEOUT        2       /* 超时错误 */
#define ATK_MW1268D_EINVAL          3       /* 参数错误 */
#define ATK_MW1268D_EBUSY           4       /* 忙错误 */
		 		

/* 操作函数 */
uint8_t atk_mw1268d_init(uint32_t baudrate, u32 secret);                                                       /* ATK-MW1268D初始化 */
void atk_mw1268d_enter_config(void);                                                                /* ATK-MW1268D模块进入配置模式 */
void atk_mw1268d_exit_config(void);                                                                 /* ATK-MW1268D模块进退出置模式 */
uint8_t atk_mw1268d_free(void);                                                                     /* 判断ATK-MW1268D模块是否空闲 */                          /* 向ATK-MW1268D模块发送AT指令 */
uint8_t atk_mw1268d_at_test(void);   
void Clear_Buffer_LORA(void);																																				/* 清空缓存 */
void lora_init(u32 secret);																																					/* 调用lora初始化函数 */
void atk_mw1268d_change_mode(atk_mw1268d_workmode_t mode);
void atk_mw1268d_change_secret(u32 secret);
uint8_t atk_mw1268d_addr_config(uint16_t addr);                                                     /* ATK-MW1268D模块设备地址配置 */
uint8_t atk_mw1268d_tpower_config(atk_mw1268d_tpower_t tpower);                                     /* ATK-MW1268D模块发射功率配置 */
uint8_t atk_mw1268d_workmode_config(atk_mw1268d_workmode_t workmode);                               /* ATK-MW1268D模块工作模式配置 */
uint8_t atk_mw1268d_tmode_config(atk_mw1268d_tmode_t tmode);                                        /* ATK-MW1268D模块发送模式配置 */
uint8_t atk_mw1268d_wlrate_channel_config(atk_mw1268d_wlrate_t wlrate, uint8_t channel);            /* ATK-MW1268D模块空中速率和信道配置 */
uint8_t atk_mw1268d_netid_config(uint8_t netid);                                                    /* ATK-MW1268D模块网络地址配置 */
uint8_t atk_mw1268d_wltime_config(atk_mw1268d_wltime_t wltime);                                     /* ATK-MW1268D模块休眠时间配置 */
uint8_t atk_mw1268d_packsize_config(atk_mw1268d_packsize_t packsize);                               /* ATK-MW1268D模块数据包大小配置 */
uint8_t atk_mw1268d_datakey_config(uint32_t datakey);                                               /* ATK-MW1268D模块数据加密密钥配置 */
uint8_t atk_mw1268d_uart_config(atk_mw1268d_uartrate_t baudrate, atk_mw1268d_uartpari_t parity);    /* ATK-MW1268D模块串口配置 */
uint8_t atk_mw1268d_lbt_config(atk_mw1268d_enable_t enable);                                        /* ATK-MW1268D模块信道检测配置 */
uint8_t atk_mw1268d_rssi_config(atk_mw1268d_enable_t enable);                                       /* ATK-MW1268D模块RSSI输出配置 */
#endif

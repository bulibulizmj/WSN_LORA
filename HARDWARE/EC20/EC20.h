#ifndef __EC20_H
#define __EC20_H
#include "sys.h"
#include "delay.h"
#include "usart.h"

/*****************采用EC20****************/
//void  EC20_Init(void);
//void EC20Send_StrData(char *bufferdata);
//void EC20Send_HexData(char *bufferdata);		
//void Clear_Buffer(void);
//void EC20Send_RecAccessMode(void);
//u8 EC20_CONNECT_SERVER_CFG_INFOR(u8 *PRODUCTKEY,u8 *DEVICENAME,u8 *DEVICESECRET);
//extern u8 EC20_MQTT_SEND_DATA(u8 *PRODUCTKEY,u8 *DEVICENAME,u8 *DATA);
//extern u8 EC20_MQTT_SEND_AUTO(u8 *PRODUCTKEY,u8 *DEVICENAME);

/*****************采用EC800****************/


#define EC800_EN	PAout(1)
void  EC800_Init(void);
void Clear_Buffer_EC800(void);//清空缓存
void EN_pin_init(void);
void reset_4g(void);
u8 EC20_CONNECT_SERVER_CFG_INFOR(u8 *CLIENTID,u8 *USERNAME,u8 *PASSWORD);
u8 EC20_CONNECT_MQTT_SERVER(u8 *CLIENTID,u8 *USERNAME,u8 *PASSWORD);
u8 EC20_MQTT_SEND_AUTO(u8 *TOPIC);
u8 EC20_MQTT_SEND_DATA(u8 *TOPIC,u8 *DATA);
void EC20Send_RecAccessMode(void);

#endif

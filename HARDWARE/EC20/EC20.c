#include "ec20.h"
#include "stdlib.h"
#include "string.h"
#include "usart.h"	


#define SERVERIP "47.109.93.42"  //EMQX服务器IP地址
#define SERVERPORT 1883  //EMQX服务器端口


int errcount = 0, i = 0;
char AtStrBuf_EC800[BUFLEN];
char *strx_EC800,*extstrx_EC800,*Readystrx_EC800;
extern char AtRxBuffer_EC800[200];
extern u16 Rxcouter_EC800;


/**
 * @brief       清空EC800接收缓存
 * @param       无
 * @retval      无
 *              
 */
void Clear_Buffer_EC800(void)//清空缓存
{
    u16 i;
    printf(AtRxBuffer_EC800);
    for(i=0;i<Rxcouter_EC800;i++)
        AtRxBuffer_EC800[i]=0;//缓存
    Rxcouter_EC800=0;
    //	IWDG_Feed();//喂狗
}


/**
 * @brief       EN引脚初始化 拉低关闭4g模块电源 拉高打开
 * @param       无
 * @retval      无
 *              
 */

void EN_pin_init(void)
{
		GPIO_InitTypeDef  GPIO_InitStructure;

		RCC_APB2PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);	 //使能PA,PD端口时钟

		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;				 //LED0-->PA.8 端口配置
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT; 		 //推挽输出
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;		 //IO口速度为50MHz
		GPIO_Init(GPIOA, &GPIO_InitStructure);					 //根据设定参数初始化GPIOA.8
		EC800_EN = 1;						 //PA.8 输出高
}


/**
 * @brief       复位4G模块,即EN拉低再拉高
 * @param       无
 * @retval      无
 *              
 */

void reset_4g(void)
{
    EC800_EN = 0;
    delay_ms(3000);
    EC800_EN = 1;
    delay_ms(100);
}

/**
 * @brief       等待模块响应
 * @param       expect_recv:期望响应
 * 							time:等待时间（单位s）
 * @retval      1 得到正确响应
 *              0 得到错误响应
 */
u8 wait_for_receive(char* expect_recv, u8 time)
{
		int count = 0;
	  strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)expect_recv);//
	  while(strx_EC800==NULL)
    {
        count++;
        delay_ms(500);
        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)expect_recv);//返回OK
        if(count>(2*time))     //防止死循环
        {
						Clear_Buffer_EC800();   
						return 0;
				}
		}
		Clear_Buffer_EC800();   
		return 1;		
}
/**
 * @brief       初始化4G模块
 * @param       无
 * @retval      无
 *              
 */

void  EC800_Init(void)
{
		reset_4g();
		delay_ms(1000);
		delay_ms(1000);
		uart2_init(115200);//和模块通信波特率921600 新的模块是115200 
		EN_pin_init();
		delay_ms(50);
    Uart2_SendStr("AT\r\n");
    delay_ms(500);
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"OK");//返回OK
    Clear_Buffer_EC800();
		errcount = 0;
    while(strx_EC800==NULL)
    {
        errcount++;
        printf("\r\n单片机正在连接到模块...\r\n");
        Clear_Buffer_EC800();
        Uart2_SendStr("AT\r\n");
        delay_ms(500);
        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"OK");//返回OK
        if(errcount>50)     //防止死循环
        {
            errcount = 0;
            reset_4g();
            __set_FAULTMASK(1); //关闭总中断
            NVIC_SystemReset(); //请求单片机重启
            break;
        }
    }

    Uart2_SendStr("ATE1\r\n"); //回显
    delay_ms(500);
    Clear_Buffer_EC800();
		errcount = 0;
    /////////////////////////////////
    Uart2_SendStr("AT+CPIN?\r\n");//检查SIM卡是否在位
    delay_ms(500);
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+CPIN: READY");//查看是否返回ready
    while(strx_EC800==NULL)
    {
			  errcount++;
        Clear_Buffer_EC800();
        Uart2_SendStr("AT+CPIN?\r\n");
        delay_ms(500);
        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+CPIN: READY");//检查SIM卡是否在位，等待卡在位，如果卡识别不到，剩余的工作就没法做了，有可能卡不可识别之后又能识别了，但模块需要复位才可以识别，这是硬件上的一个问题
				if(errcount>50)     //需要软件来防止死循环
        {
            errcount = 0;
						printf("reset 4g\r\n");
            reset_4g();
            __set_FAULTMASK(1); //关闭总中断
            NVIC_SystemReset(); //请求单片机重启
            break;
        }
    }
    Clear_Buffer_EC800();
    ///////////////////////////////////////
    Uart2_SendStr("AT+CSQ\r\n"); //检查CSQ, 信号质量，最大值31
    delay_ms(500);
    Clear_Buffer_EC800();
    Uart2_SendStr("ATI\r\n"); //检查模块的版本号
    delay_ms(500);
    Clear_Buffer_EC800();


    ///////////////////////////////////
    Uart2_SendStr("AT+CREG?\r\n");//查看是否注册GSM网络
    delay_ms(500);
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+CREG: 0,1");//返回正常
    extstrx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+CREG: 0,5");//返回正常，漫游
		errcount = 0;
    while(strx_EC800==NULL&&extstrx_EC800==NULL)
    {
        Clear_Buffer_EC800();
			  errcount++;
        Uart2_SendStr("AT+CREG?\r\n");//查看是否注册GSM网络
        delay_ms(500);
        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+CREG: 0,1");//返回正常
        extstrx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+CREG: 0,5");//返回正常，漫游
			  if(errcount>50)     //防止死循环
        {
            errcount = 0;
            reset_4g();
            __set_FAULTMASK(1); //关闭总中断
            NVIC_SystemReset(); //请求单片机重启
            break;
        }
    }
    Clear_Buffer_EC800();

    Uart2_SendStr("AT+COPS?\r\n");//查看注册到哪个运营商，支持移动 联通 电信
    delay_ms(500);
    Clear_Buffer_EC800();
    Uart2_SendStr("AT+QICLOSE=0\r\n");//关闭socket连接
    delay_ms(1000);
    Clear_Buffer_EC800();
    Uart2_SendStr("AT+CIMI\r\n");//获取卡号，类似是否存在卡的意思，比较重要。
    delay_ms(1000);
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"OK");//只要卡不错误 基本就成功
    if(strx_EC800)
    {
        printf("============\r\n我的卡号是 : %s \r\n===============\r\n",AtRxBuffer_EC800);
        delay_ms(1000);
        Clear_Buffer_EC800();

    }
    else
    {
        printf("卡错误 : %s \r\n",AtRxBuffer_EC800);
        delay_ms(300);
        Clear_Buffer_EC800();
    }
    Clear_Buffer_EC800();

    Uart2_SendStr("AT+QIDEACT=1\r\n");//去激活
    delay_ms(1000);
    Clear_Buffer_EC800();

    Uart2_SendStr("AT+QIACT=1\r\n");//激活
    delay_ms(500);
    Clear_Buffer_EC800();

    Uart2_SendStr("AT+CGATT=1\r\n");//激活网络，PDP
    delay_ms(300);
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"OK");//返OK
    Clear_Buffer_EC800();

    Uart2_SendStr("AT+CGATT?\r\n");//查询激活状态
    delay_ms(300);
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+CGATT: 1");//返1 表明激活成功 获取到IP地址了
    Clear_Buffer_EC800();
    errcount = 0;
    while(strx_EC800==NULL)
    {
        errcount++;
        Clear_Buffer_EC800();
        Uart2_SendStr("AT+CGATT?\r\n");//获取激活状态
        delay_ms(300);
        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+CGATT: 1");//返回1,表明注网成功
        if(errcount>100)     //防止死循环
        {
            errcount = 0;
            reset_4g();
            __set_FAULTMASK(1); //关闭总中断
            NVIC_SystemReset(); //请求单片机重启
            break;
        }
    }

    Uart2_SendStr("AT+CGPADDR\r\n");//获取当前卡的IP地址
    delay_ms(500);
    Clear_Buffer_EC800();
}



/**
 * @brief       连接EMQX服务器平台
 * @param       PRODUCTKEY
 *				DEVICENAME
 *				DEVICESECRET
 * @retval      0 连接成功
 *				1 连接失败
 *              
 */
u8 EC20_CONNECT_MQTT_SERVER(u8 *CLIENTID,u8 *USERNAME,u8 *PASSWORD)
{
    Uart2_SendStr("AT+QIDEACT=1\r\n"); //关闭当前连接
    delay_ms(500);
    Clear_Buffer_EC800();
    Uart2_SendStr("AT+QMTCLOSE=0\r\n"); //关闭MQTT客户端
    delay_ms(500);
    Clear_Buffer_EC800();
    Uart2_SendStr("AT+QMTDISC=0\r\n");//关闭和MQTT服务器的所有连接
    delay_ms(500);
    Clear_Buffer_EC800();

    //打开EMQX的连接
    memset(AtStrBuf_EC800,0,BUFLEN);
    sprintf(AtStrBuf_EC800,"AT+QMTOPEN=0,\"%s\",%d\r\n",SERVERIP,SERVERPORT);
    Uart2_SendStr(AtStrBuf_EC800);
    delay_ms(300);
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+QMTOPEN: 0,0");//返OK表示配置成功了
    errcount = 0;
    while(strx_EC800==NULL)
    {
        errcount++;
//				Clear_Buffer_EC800();
//				Uart2_SendStr("AT+QMTOPEN=0,\"iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883\r\n");
        delay_ms(200);
        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+QMTOPEN: 0,0");//返回1,表明注网成功
        if(errcount>100)     //防止死循环
        {
            errcount = 0;
            reset_4g();
            __set_FAULTMASK(1); //关闭总中断
            NVIC_SystemReset(); //请求单片机重启
            break;
        }
    }
    Clear_Buffer_EC800();

    //连接到EMQX服务器
    memset(AtStrBuf_EC800,0,BUFLEN);
    sprintf(AtStrBuf_EC800,"AT+QMTCONN=0,\"%s\",\"%s\",\"%s\"\r\n",CLIENTID,USERNAME,PASSWORD);
    Uart2_SendStr(AtStrBuf_EC800);
    delay_ms(3000);
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+QMTCONN: 0,0,0");//返OK表示配置成功了
    if(strx_EC800)
    {
       ;
    } 
		else  
		{
				Clear_Buffer_EC800();
				return 1;
		}
    Clear_Buffer_EC800();

    // //订阅到阿里云
    // memset(AtStrBuf_EC800,0,BUFLEN);
    // sprintf(AtStrBuf_EC800,"AT+QMTSUB=0,1,\"/%s/%s/user/get\",0 \r\n",PRODUCTKEY,DEVICENAME);
    // Uart2_SendStr(AtStrBuf_EC800);
    // delay_ms(1000);
    // strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"+QMTSUB: 0,1,0,1");//返OK表示配置成功了
    if(strx_EC800)
    {
        printf("阿里云物联网平台连接成功\r\n");
    }
    else  return 1;
    Clear_Buffer_EC800();
    printf("设备已经连接到阿里云,准备发送数据 [..]\r\n");
    //EC20_MQTT_SEND_AUTO("sensor/data"); //测试发送数据
    return 0;
}

/**
 * @brief       调用EC20_CONNECT_MQTT_SERVER函数，连接阿里云物联网平台
 * @param       PRODUCTKEY
 *							DEVICENAME
 *							DEVICESECRET
 * @retval      res:0 连接成功
 *									1 连接失败
 *              
 */
u8 EC20_CONNECT_SERVER_CFG_INFOR(u8 *CLIENTID,u8 *USERNAME,u8 *PASSWORD)
{
    u8 res;
    res=EC20_CONNECT_MQTT_SERVER(CLIENTID,USERNAME,PASSWORD);
    return res;
}

int MQTTVAL=0;
/**
 * @brief       向阿里云物联网平台发送测试数据（自动发光照度）
 * @param       TOPIC		
 *
 * @retval      0 发送成功
 *				1 发送失败
 *              
 */
u8 EC20_MQTT_SEND_AUTO(u8 *TOPIC)
{
    memset(AtStrBuf_EC800,0,BUFLEN); //发送数据命令
    //AT+QMTPUB=0,0,0,0,"/sys/a18dtRetCT0/BC26TEST/thing/event/property/post"
    //AT+QMTPUB=0,0,0,0,"sensor/data"
    char data_len_str[] = "{\"temp\":25.5}";
    sprintf(AtStrBuf_EC800,"AT+QMTPUBEX=0,1,1,0,\"%s\",%d\r\n",TOPIC,strlen(data_len_str));
    Uart2_SendStr(AtStrBuf_EC800);
    delay_ms(1000);
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)">");//模块反馈的字符串
    errcount = 0;
    Clear_Buffer_EC800();
    while(strx_EC800==NULL)
    {
        errcount++;
        delay_ms(300);
				strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)">");//模块反馈的字符串
				if(errcount>100)     //防止死循环
        {
            errcount = 0;
            reset_4g();
            __set_FAULTMASK(1); //关闭总中断
            NVIC_SystemReset(); //请求单片机重启
            break;
        }
    }

    MQTTVAL++;
    if(MQTTVAL > 900)
        MQTTVAL = 0;

    // memset(AtStrBuf_EC800,0,BUFLEN); //发送数据命令
    // sprintf(AtStrBuf_EC800,"",MQTTVAL);
    Uart2_SendStr(data_len_str);
    delay_ms(300);

    while((USART2->SR&0X40)==0);//等待发送完成
    USART2->DR = (u8) 0x1a;
    delay_ms(300);
    EC20Send_RecAccessMode();
    Clear_Buffer_EC800();
    printf("系统数据发送成功  [OK]\r\n");
    return 0;
}


/**
 * @brief       向阿里云物联网平台发送自定义数据
 * @param       TOPIC
 *				DATA
 *
 * @retval      0 发送成功
 *				1 发送失败
 *              
 */
u8 EC20_MQTT_SEND_DATA(u8 *TOPIC,u8 *DATA)
{
    memset(AtStrBuf_EC800,0,BUFLEN); //发送数据命令
    //AT+QMTPUB=0,0,0,0,"sensor/data"
    sprintf(AtStrBuf_EC800,"AT+QMTPUBEX=0,1,1,0,\"%s\",%d\r\n",TOPIC,strlen((const char *)DATA));
    Uart2_SendStr(AtStrBuf_EC800);
    delay_ms(1000);
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)">");//模块反馈的字符串
    errcount = 0;
    while(strx_EC800==NULL)
    {
        errcount++;
        delay_ms(300);
        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)">");//模块反馈的字符串
        if(errcount>100)     //防止死循环
        {
            errcount = 0;
            reset_4g();
            __set_FAULTMASK(1); //关闭总中断
            NVIC_SystemReset(); //请求单片机重启
            break;
        }
    }

    Uart2_SendStr((char *)DATA);
    delay_ms(300);
    while((USART2->SR&0X40)==0);//等待发送完成
    USART2->DR = (u8) 0x1a;
    delay_ms(300);
    EC20Send_RecAccessMode();
    Clear_Buffer_EC800();
    printf("用户数据发送成功  [OK]\r\n");
    return 0;
}


/**
 * @brief       接收阿里云的数据后，执行相应的操作
 * @param       无
 *												
 * @retval      无
 *              
 */
void EC20Send_RecAccessMode(void)
{
    //控制LED 灯1开csled01 灯1关csled00 灯2开csled11......以此类推
    strx_EC800 = NULL;
    strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"QMTRECV");
//    if(strx_EC800)
//    {
//        printf("+++接收到上位机下发指令了+++\r\n");
//        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"csled00");//接收到服务器下发数据
//        if(strx_EC800)
//        {
//            GPIO_SetBits(GPIOC,GPIO_Pin_4);//LED1 关闭
//        }
//        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"csled10");//接收到服务器下发数据
//        if(strx_EC800)
//        {
//            GPIO_SetBits(GPIOA,GPIO_Pin_7);//LED2 关闭
//        }
//        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"csled20");//接收到服务器下发数据
//        if(strx_EC800)
//        {
//            GPIO_SetBits(GPIOC,GPIO_Pin_0);//LED3 关闭
//        }

//        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"csled01");//接收到服务器下发数据
//        if(strx_EC800)
//        {
//            GPIO_ResetBits(GPIOC,GPIO_Pin_4);//LED1 开启
//        }
//        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"csled11");//接收到服务器下发数据
//        if(strx_EC800)
//        {
//            GPIO_ResetBits(GPIOA,GPIO_Pin_7);//LED2 开启

//        }
//        strx_EC800=strstr((const char*)AtRxBuffer_EC800,(const char*)"csled21");//接收到服务器下发数据
//        if(strx_EC800)
//        {
//            GPIO_ResetBits(GPIOC,GPIO_Pin_0);//LED3 开启
//        }
//    }
    Clear_Buffer_EC800();
}

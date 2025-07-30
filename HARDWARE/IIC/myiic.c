#include "myiic.h"
#include "delay.h"
#include "usart.h"
//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32F407开发板
//IIC 驱动代码	   
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2014/5/6
//版本：V1.0
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved									  
////////////////////////////////////////////////////////////////////////////////// 	

//初始化IIC
void IIC_Init(void)
{			
	GPIO_InitTypeDef  GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOA, ENABLE);//使能GPIOB时钟

	//GPIOB1,B2初始化设置 B2为SDA
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//普通输出模式
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;//开漏输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
	GPIO_Init(GPIOC, &GPIO_InitStructure);//初始化
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
	GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化
	IIC_SCL=1;
	IIC_SDA=1;
//	Add_SDO = 0;//高电平I2C地址为0X77
}




//产生IIC起始信号
void IIC_Start(void)
{
	SDA_OUT();     //sda线输出
	IIC_SDA=1;	  	  
	IIC_SCL=1;
	delay_us(4);
 	IIC_SDA=0;//START:when CLK is high,DATA change form high to low 
	delay_us(4);
	IIC_SCL=0;//钳住I2C总线，准备发送或接收数据 
}	  
//产生IIC停止信号
void IIC_Stop(void)
{
	SDA_OUT();//sda线输出
	IIC_SCL=0;
	IIC_SDA=0;//STOP:when CLK is high DATA change form low to high
 	delay_us(4);
	IIC_SCL=1; 
	IIC_SDA=1;//发送I2C总线结束信号
	delay_us(2);							   	
}
//等待应答信号到来，读应答信号
//返回值：1，接收应答失败
//        0，接收应答成功
u8 IIC_Wait_Ack(void)
{
	u8 ucErrTime=0;
	SDA_IN();      //SDA设置为输入  
	IIC_SDA=1;delay_us(1);	   
	IIC_SCL=1;delay_us(1);	 
	while(READ_SDA)//1为接收应答失败
	{
		ucErrTime++;
		if(ucErrTime>250)
		{
			IIC_Stop();
			return 1;
		}
	}
	IIC_SCL=0;//时钟输出0 	   
	return 0;  
} 
//产生ACK应答
void IIC_Ack(void)
{
	IIC_SCL=0;
	SDA_OUT();
	IIC_SDA=0;//0表示发送应答
	delay_us(2);
	IIC_SCL=1;
	delay_us(2);
	IIC_SCL=1;
}
//不产生ACK应答	,非应答信号	    
void IIC_NAck(void)
{
	IIC_SCL=0;
	SDA_OUT();
	IIC_SDA=1;//1表示发送非应答信号
	delay_us(2);
	IIC_SCL=1;
	delay_us(2);
	IIC_SCL=1;
}					 				     
//IIC发送一个字节
//返回从机有无应答
//1，有应答
//0，无应答			  
void IIC_Send_Byte(u8 txd)
{                        
    u8 t;   
		SDA_OUT(); 	    
    IIC_SCL=0;//拉低时钟开始数据传输
    for(t=0;t<8;t++)
    {              
        IIC_SDA=(txd&0x80)>>7;
        txd<<=1; 	  
				delay_us(2);   //对TEA5767这三个延时都是必须的
				IIC_SCL=1;
				delay_us(2); 
				IIC_SCL=0;	
				delay_us(2);
    }	 
} 	    
//读1个字节，ack=1时，发送ACK，ack=0，发送nACK   发送nACK表示告诉从机不想继续读数据。发送ACK则表示连续读从机数据
u8 IIC_Read_Byte(unsigned char ack)
{
	u8 i,receive=0;
	SDA_IN();//SDA设置为输入
    for(i=0;i<8;i++ )
	{
        IIC_SCL=0; 
        delay_us(4);
				IIC_SCL=1;
        receive<<=1;
        if(READ_SDA)receive++;   
		delay_us(1); 
    }					 
    if (!ack)
        IIC_NAck();//发送nACK
    else
        IIC_Ack(); //发送ACK   
    return receive;
}

//IIC指定地址写一个字节 
//devaddr:器件IIC地址
//reg:寄存器地址
//data:数据
//返回值:0,正常
//    其他,错误代码
u8 IIC_Write_One_Byte(u8 addr,u8 reg,u8 data)
{
    IIC_Start();
    IIC_Send_Byte((addr<<1)|0); //发送器件地址+写命令
    if(IIC_Wait_Ack())          //等待应答
    {
        IIC_Stop();
        return 1;
    }
    IIC_Send_Byte(reg);         //写寄存器地址
    IIC_Wait_Ack();             //等待应答
    IIC_Send_Byte(data);        //发送数据
    if(IIC_Wait_Ack())          //等待ACK
    {
        IIC_Stop();
        return 1;
    }
    IIC_Stop();
    return 0;
}

//IIC指定地址读一个字节 
//reg:寄存器地址 
//返回值:读到的数据
u8 IIC_Read_One_Byte(u8 addr,u8 reg)
{
    u8 res,ack1;
    IIC_Start();
    IIC_Send_Byte((addr<<1)|0); //发送器件地址+写命令
    IIC_Wait_Ack();             //等待应答
    IIC_Send_Byte(reg);         //写寄存器地址
    IIC_Wait_Ack();             //等待应答
	  IIC_Start();                
    IIC_Send_Byte((addr<<1)|1); //发送器件地址+读命令
    IIC_Wait_Ack();             //等待应答
    res=IIC_Read_Byte(0);		//读数据,发送nACK  
    IIC_Stop();                 //产生一个停止条件
    return res;  
}
//u8 IIC_Read_One_Byte(u8 addr,u8 reg)
//{
//    u8 res,ack1;
//    IIC_Start();
//    IIC_Send_Byte((addr<<1)|0); //发送器件地址+写命令
//		ack1 = IIC_Wait_Ack();
//    if(!ack1)             //等待应答
//		{
//			printf("order name successful\n");
//		}
//		else
//		{
//			printf("order name failed\n");
//		}
//    IIC_Send_Byte(reg);         //写寄存器地址
//    ack1 = IIC_Wait_Ack();             //等待应答
//		if(!ack1)             //等待应答
//		{
//			printf("order reg successful\n");
//		}
//		else
//		{
//			printf("order reg failed\n");
//		}
//	  IIC_Start();                
//    IIC_Send_Byte((addr<<1)|1); //发送器件地址+读命令
//    ack1 = IIC_Wait_Ack();             //等待应答
//		if(!ack1)             //等待应答
//		{
//			printf("read successful\n");
//		}
//		else
//		{
//			printf("read failed\n");
//		}
//    res=IIC_Read_Byte(0);		//读数据,发送nACK  
//    IIC_Stop();                 //产生一个停止条件
//    return res;  
//}

//连续读多个字节
//addr:起始地址
//rbuf:读数据缓存
//len:数据长度
void IIC_Read_Len(u8 addr,u8 reg,u8 len,u8 *rbuf)
{
	int i=0;
	
	IIC_Start();
  IIC_Send_Byte((addr<<1)|0); //发送器件地址+写命令
  IIC_Wait_Ack();             //等待应答
  IIC_Send_Byte(reg);         //写寄存器地址
  IIC_Wait_Ack();             //等待应答
	IIC_Start();                
  IIC_Send_Byte((addr<<1)|1); //发送器件地址+读命令
  IIC_Wait_Ack();             //等待应答
	
	for(i=0; i<len; i++)
	{
		if(i==len-1)
		{
			rbuf[i]=IIC_Read_Byte(0); //发送非应答，即不发送应答信号，表示读取结束，主机重新掌握总线控制权                                         //最后一个字节不应答
		}
		else
		{
			rbuf[i]=IIC_Read_Byte(1);
		}
	}
	IIC_Stop( );	
}
//连续读一个位置多个字节
//ack:1需要发送应答；0不需要应答
//recvdata:数据缓存首地址，一般为u8数组名
//num:数据长度
uint32_t IIC_ReadSHT45_Len(uint8_t ack,uint8_t* recvdata,uint8_t num)
{
	uint32_t  data=0;
	while(num--)
	{
		*recvdata++ = IIC_Read_Byte(ack);
	}
	return  data;
}



















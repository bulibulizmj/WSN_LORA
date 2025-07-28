#include <math.h>
#include "stdbool.h"
#include "delay.h"
#include "sht45.h"
#include "myiic.h"
#include "usart.h"
/********************************************************************************	 

********************************************************************************/
uint32_t SHT45_Data1[2]={0};

void sht45init(void)
{
	IIC_Init();
}
//单字节CRC8校验
unsigned char cal_table_high_first(unsigned char value)
{
    unsigned char i, crc;
    crc = value;
    /* 数据往左移了8位，需要计算8次 */
    for (i=8; i>0; --i)
    { 
        if (crc & 0x80)  /* 判断最高位是否为1 */
        {
        /* 最高位为1，不需要异或，往左移一位，然后与0x31异或 */
        /* 0x31(多项式：x8+x5+x4+1，100110001)，最高位不需要异或，直接去掉 */
            crc = (crc << 1) ^ 0x31;        }
        else
        {
            /* 最高位为0时，不需要异或，整体数据往左移一位 */
            crc = (crc << 1);
        }
    }
 
    return crc;
}
//多字节CRC8校验
unsigned char crc_high_first(unsigned char *ptr, unsigned char len)
{
    unsigned char i; 
    unsigned char crc=0xFF ; /* 计算的初始crc值 */ 
 
    while(len--)
    {
        crc ^= *ptr++;  /* 每次先与需要计算的数据异或,计算完指向下一数据 */  
        for (i=8; i>0; --i)   /* 下面这段计算过程与计算一个字节crc一样 */  
        { 
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc = (crc << 1);
        }
    }
 
    return (crc); 
}

//返回值0读取成功；1读取失败
uint8_t SHT45_ReadPdata(uint8_t ack,float* T,float* H )
{	
		uint8_t arr[6]={0};
	  int32_t CRC1=0XFF;
	  int32_t CRC2=0XFF;
	
		//开启温度转换
		IIC_Start();
		IIC_Send_Byte((0x44<<1)|0); //发送器件地址+写命令，0x38为AHT20的7位地址,0x77为BMP280地址
		delay_us(2);
    if(IIC_Wait_Ack())          //等待ACK
    {
        IIC_Stop();
        return 1;
    }

		IIC_Send_Byte (0xFD);//FD最高精度
		delay_us(2);
    if(IIC_Wait_Ack())          //等待ACK
    {
        IIC_Stop();
        return 1;
    }
		IIC_Stop();		
		delay_ms(20);
		//读取温度
		IIC_Start();
    IIC_Send_Byte (0x89);	//0x44 <<1 +1读命令
		delay_us(2);
    if(IIC_Wait_Ack())          //等待ACK
    {
        IIC_Stop();
        return 1;
    }
    IIC_ReadSHT45_Len(ack,arr,6);		
		IIC_Stop();
		//for(i=0;i<6;i++)printf("%x\r\n",arr[i]);
		SHT45_Data1[0] =arr[0]<<8|arr[1];
		*T = (-45 + 175*(SHT45_Data1[0])/65535.0);
		SHT45_Data1[1] =arr[3]<<8|arr[4];
		*H =(-6 + 125 *( SHT45_Data1[1])/65535.0);
		
		CRC1 = crc_high_first(arr,2);//CRC8校验
    if(CRC1 == arr[2]);	
		else
			return 1;		
		CRC2 = crc_high_first(arr+3,2);//CRC8校验
    if(CRC2 == arr[5]) ;
		else
			return 1;
		
    return 0;          
}


//返回原始值
uint32_t SHT45_ReadRawData(uint8_t ack)
{	
		uint8_t arr[6]={0};
    uint32_t raw_data = 0;
	  int32_t CRC1=0XFF;
	  int32_t CRC2=0XFF;
	
		//开启温度转换
		IIC_Start();
		IIC_Send_Byte((0x44<<1)|0); //发送器件地址+写命令，0x38为AHT20的7位地址,0x77为BMP280地址
		delay_us(2);
    if(IIC_Wait_Ack())          //等待ACK
    {
        IIC_Stop();
        return 1;
    }

		IIC_Send_Byte (0xFD);//FD最高精度
		delay_us(2);
    if(IIC_Wait_Ack())          //等待ACK
    {
        IIC_Stop();
        return 1;
    }
		IIC_Stop();		
		delay_ms(20);
		//读取温度
		IIC_Start();
    IIC_Send_Byte (0x89);	//0x44 <<1 +1读命令
		delay_us(2);
    if(IIC_Wait_Ack())          //等待ACK
    {
        IIC_Stop();
        return 1;
    }
    IIC_ReadSHT45_Len(ack,arr,6);		
		IIC_Stop();
		//for(i=0;i<6;i++)printf("%x\r\n",arr[i]);
    raw_data = arr[0]<<8|arr[1] + ((uint32_t)(arr[3]<<8|arr[4]) << 16);
		
		CRC1 = crc_high_first(arr,2);//CRC8校验
    if(CRC1 == arr[2]);	
		else
			return 1;		
		CRC2 = crc_high_first(arr+3,2);//CRC8校验
    if(CRC2 == arr[5]) ;
		else
			return 1;
		
    return raw_data;          
}

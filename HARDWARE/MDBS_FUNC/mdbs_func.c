#include "mdbs_func.h"

//#define soilsensor_number 1  //土壤温湿度传感器数量

/* 
说明：
    接收“读保持寄存器”的结果
    命令0X03
返回：
    res_OK 正确
    res_ERR1 其他错误
    res_ERR2 地址不符
    res_ERR3 无反馈
		res_CRCERR CRC错误
*/
u8 mb_recv_readHoldingReg_all( _mbdata_st *mbp, u8 functioncode)
{
		u16 temp;//存放CRC校验码
		u16 crc_r;//存放接收到的CRC校验码
		u8 i;
		Mdbus_CTRL = 0;
		//delay_ms(10);
    if( serialRXbuf_st.len == 0 ) return res_ERR3;
 
    serialRXbuf_st.len=0;
    if( mbp->addr == serialRXbuf_st.buf[0] )
    {
		//从这里开始对functioncode分类
			if(functioncode == serialRXbuf_st.buf[1] )
			{
				if(functioncode == 0x03)
				{
						for(i=0;i<serialRXbuf_st.buf[2]/2;i++)
						{
						mbp->buf[i]= (u16)(serialRXbuf_st.buf[i*2+3]<<8) + serialRXbuf_st.buf[i*2+4];
						}
            mbp->len =  serialRXbuf_st.buf [2]/2;//寄存器数
						temp=mc_check_crc16(serialRXbuf_st.buf, 3 + serialRXbuf_st.buf [2]);//0x03功能码发送时，校验长度6个字节
						crc_r =(u16)(serialRXbuf_st.buf[serialRXbuf_st.buf[2] + 3] << 8) + serialRXbuf_st.buf[serialRXbuf_st.buf[2] + 4];
						if(temp != crc_r) return res_CRCERR;
						else return res_OK;	 
				}
				else if(functioncode == 0x06)
				{
						temp=mc_check_crc16(serialRXbuf_st.buf, 6);//0x06功能码发送时，校验长度6个字节
						crc_r =(u16)(serialRXbuf_st.buf[6] << 8) + serialRXbuf_st.buf[7];
						if(temp != crc_r) return res_CRCERR;
						else return res_OK;	 
				}
			}
      else return res_ERR1;
         
    }
    return res_ERR2; 
}
u8 mb_recv_readHoldingReg( _mbdata_st *mbp)
{ 
		u16 temp;//存放CRC校验码
		u16 crc_r;//存放接收到的CRC校验码
		u8 i;
		Mdbus_CTRL = 0;
		//delay_ms(10);
    if( serialRXbuf_st.len == 0 ) return res_ERR3;
 
    serialRXbuf_st.len=0;
    if( mbp->addr == serialRXbuf_st.buf[0] )
    {
				if( 0x03 == serialRXbuf_st.buf[1] )
				{ 
					 for(i=0;i<serialRXbuf_st.buf[2]/2;i++)
					 {
							mbp->buf[i]= (u16)(serialRXbuf_st.buf[i*2+3]<<8) + serialRXbuf_st.buf[i*2+4];
					 }
					 mbp->len =  serialRXbuf_st.buf [2]/2;//寄存器数
					 temp=mc_check_crc16(serialRXbuf_st.buf, 3 + serialRXbuf_st.buf [2]);//0x03功能码发送时，校验长度6个字节
					 crc_r =(u16)(serialRXbuf_st.buf[serialRXbuf_st.buf[2] + 3] << 8) + serialRXbuf_st.buf[serialRXbuf_st.buf[2] + 4];
					 if(temp != crc_r)
					 {
							return res_CRCERR;
					 }
					 else
					 {
							return res_OK;
					 }	 
				}
         else
         {
             return res_ERR1;
         }
    }
    return res_ERR2; 
}


//sp3485控制输入输出使能引脚初始化
void Mdbus_CTRL_Init(void)
{    	 
  GPIO_InitTypeDef  GPIO_InitStructure;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);//使能GPIOF时钟

  //
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;//普通输出模式
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;//上拉
  GPIO_Init(GPIOB, &GPIO_InitStructure);//初始化
	
	GPIO_SetBits(GPIOB, GPIO_Pin_6);//GPIOF9,F10设置高，灯灭

}

/*
发送"写保持寄存器"，命令0X03
*/

void mb_sent_writeHoldingReg( const _mbdata_st  mbp)
{ 
	u8 len = mbp.len;
	u16 temp;//存放CRC校验码
	Mdbus_CTRL = 1;
	delay_ms(10);
	serialTXbuf_st.buf[0] = mbp.addr;//0x01
	serialTXbuf_st.buf[1] = 0x03;//功能码
	serialTXbuf_st.buf[2] = mbp.start>>8;//起始地址高八位00
	serialTXbuf_st.buf[3] = mbp.start;//起始地址低八位00
	serialTXbuf_st.buf[4] = 0x00;//00
	serialTXbuf_st.buf[5] =  len;//寄存器个数01
//	serialTXbuf_st.buf[6] =  0xD5;//CRC
//	serialTXbuf_st.buf[7] =  0xCA;   
	temp=mc_check_crc16(serialTXbuf_st.buf, 6);//0x03功能码发送时，校验长度6个字节
	serialTXbuf_st.buf[6] = temp>>8;    //CRC16低位在前，mc_check_crc16函数已自动完成调换
	serialTXbuf_st.buf[7] = temp;//高

	myUSART_Sendarr(  USART3,   serialTXbuf_st.buf ,  8) ;
	WaitForTransmitComplete(USART3) ; //发送完成
	Mdbus_CTRL = 0;
 }

/*
发送"写保持寄存器"，命令为functioncode 0x03 0x06
*/

void mb_sent_writeHoldingReg_all( const _mbdata_st mbp, u8 functioncode)
{ 
	u8 len = mbp.len;
	u16 temp;//存放CRC校验码
	Mdbus_CTRL = 1;
	delay_ms(10);
	serialTXbuf_st.buf[0] = mbp.addr;//土壤温湿度传感器地址0x01，雨量传感器地址0x02
	serialTXbuf_st.buf[1] = functioncode;//功能码
	serialTXbuf_st.buf[2] = mbp.start>>8;//起始地址高八位00
	serialTXbuf_st.buf[3] = mbp.start;//起始地址低八位00
	serialTXbuf_st.buf[4] = 0x00;//00
	serialTXbuf_st.buf[5] =  len;//寄存器个数01//////要改
//	serialTXbuf_st.buf[6] =  0xD5;//CRC
//	serialTXbuf_st.buf[7] =  0xCA;   
	temp = mc_check_crc16(serialTXbuf_st.buf, 6);//0x03功能码发送时，校验长度6个字节
	serialTXbuf_st.buf[6] = temp>>8;    //CRC16低位在前，mc_check_crc16函数已自动完成调换
	serialTXbuf_st.buf[7] = temp;//高

	myUSART_Sendarr(  USART3,   serialTXbuf_st.buf ,  8) ;
	WaitForTransmitComplete(USART3) ; //发送完成
	Mdbus_CTRL = 0;
 }

/*
查询当前土壤温湿度，functioncode 0x03 
参数DeviceAdd：设备地址
返回值为：土壤温湿度原始数据
*/
u32 CurrentSoilstate(u8 DeviceAdd)
{
	u8 errcount = 0;
	//定义要发送的信息
	_mbdata_st mbp_s;
	_mbdata_st mbp_r; 
	mbp_s.addr = DeviceAdd;
	mbp_s.start = 0x0000;
	mbp_s.len = 0x02;//如果是0x06则此处意义不是读取寄存器个数
	
	mbp_r.addr = mbp_s.addr;
	mbp_r.start = mbp_s.start;
	mb_sent_writeHoldingReg_all(mbp_s, 0x03);
	delay_ms(10);//进入中断函数，防止直接进入while循环
	while(mb_recv_readHoldingReg_all(&mbp_r,0x03) != res_OK)
	{
		delay_ms(20);
		mb_sent_writeHoldingReg_all(mbp_s, 0x03);
		errcount++;
		if(errcount>20) {errcount = 0; printf("soilsensor error!\r\n"); return 0;}
	}//接收到数据并保存在mbp_r中,若错误则间隔20ms重复发送
	//printf("raw data:%d\r\n", mbp_r.buf[0]);
	return (mbp_r.buf[0]<<16) + mbp_r.buf[1];//返回的雨量值扩大了十倍
}
 
/*
查询当前雨量值，functioncode 0x03 
返回值为 当前雨量值*10
*/
u16 CurrentPrecipitation(void)
{
	u8 errcount = 0;
	//定义要发送的信息
	_mbdata_st mbp_s;
	_mbdata_st	mbp_r; 
	mbp_s.addr = 0x01;
	mbp_s.start = 0x0000;
	mbp_s.len = 0x01;//如果是0x06则此处意义不是读取寄存器个数
	
	mbp_r.addr = mbp_s.addr;
	mbp_r.start = mbp_s.start;
	mb_sent_writeHoldingReg_all(mbp_s, 0x03);
	delay_ms(10);//进入中断函数，防止直接进入while循环
	while(mb_recv_readHoldingReg_all(&mbp_r,0x03) != res_OK)
	{
		delay_ms(20);
		mb_sent_writeHoldingReg_all(mbp_s, 0x03);
		errcount++;
		if(errcount>20)  {errcount = 0; printf("precipitation error!\r\n"); break;}
	}//接收到数据并保存在mbp_r中,若错误则间隔20ms重复发送
//	printf("raw data:%d\r\n", mbp_r.buf[0]);
	return mbp_r.buf[0];//返回的雨量值扩大了十倍
}
/*
清除雨量数据，functioncode 0x06
返回值：1清除成功 0清除失败
*/
bool CleanPrecipitation(void)
{
	u8 errcount = 0;
	//定义要发送的信息
	_mbdata_st mbp_s;
	_mbdata_st	mbp_r; 
	mbp_s.addr = 0x01;
	mbp_s.start = 0x0000;
	mbp_s.len = 0x5A;//0x5A为往0x00寄存器写入清除雨量数据命令
	
	mbp_r.addr = mbp_s.addr;
	mbp_r.start = mbp_s.start;
	mb_sent_writeHoldingReg_all(mbp_s, 0x06);
	delay_ms(10);//进入中断函数，防止直接进入while循环
	while(mb_recv_readHoldingReg_all(&mbp_r,0x06) != res_OK)
	{
		delay_ms(20);
		mb_sent_writeHoldingReg_all(mbp_s, 0x06);
		errcount++;
		if(errcount>20) {errcount = 0; return 0;}
	}//接收到数据并保存在mbp_r中,若错误则间隔20ms重复发送
	return 1;
}

/*
查询当前太阳辐射值，functioncode 0x03 
返回值为 当前太阳辐射值
*/
u16 CurrentRadiation(void)
{
	u8 errcount = 0;
	//定义要发送的信息
	_mbdata_st mbp_s;
	_mbdata_st	mbp_r; 
	mbp_s.addr = 0x04;
	mbp_s.start = 0x0000;
	mbp_s.len = 0x01;//如果是0x06则此处意义不是读取寄存器个数
	
	mbp_r.addr = mbp_s.addr;
	mbp_r.start = mbp_s.start;
	mb_sent_writeHoldingReg_all(mbp_s, 0x03);
	delay_ms(10);//进入中断函数，防止直接进入while循环
	while(mb_recv_readHoldingReg_all(&mbp_r,0x03) != res_OK)
	{
		delay_ms(20);
		mb_sent_writeHoldingReg_all(mbp_s, 0x03);
		errcount++;
		if(errcount>20)  {errcount = 0; printf("Radiation error!\r\n"); break;}
	}//接收到数据并保存在mbp_r中,若错误则间隔20ms重复发送
//	printf("raw data:%d\r\n", mbp_r.buf[0]);
	return mbp_r.buf[0];//返回的风向角度值扩大了十倍,保留一位小数
}

/*
查询当前风速值，functioncode 0x03 
返回值为 当前风速值*10
*/
u16 CurrentWindsSpeed(void)
{
	u8 errcount = 0;
	//定义要发送的信息
	_mbdata_st mbp_s;
	_mbdata_st	mbp_r; 
	mbp_s.addr = 0x02;
	mbp_s.start = 0x0000;
	mbp_s.len = 0x01;//如果是0x06则此处意义不是读取寄存器个数
	
	mbp_r.addr = mbp_s.addr;
	mbp_r.start = mbp_s.start;
	mb_sent_writeHoldingReg_all(mbp_s, 0x03);
	delay_ms(10);//进入中断函数，防止直接进入while循环
	while(mb_recv_readHoldingReg_all(&mbp_r,0x03) != res_OK)
	{
		delay_ms(20);
		mb_sent_writeHoldingReg_all(mbp_s, 0x03);
		errcount++;
		if(errcount>20)  {errcount = 0; printf("WindsSpeed error!\r\n"); break;}
	}//接收到数据并保存在mbp_r中,若错误则间隔20ms重复发送
//	printf("raw data:%d\r\n", mbp_r.buf[0]);
	return mbp_r.buf[0];//返回的风向角度值扩大了十倍,保留一位小数
}

/*
查询当前风向值，functioncode 0x03 
返回值为 当前风向角度值*10
*/
u16 CurrentWindsDirection(void)
{
	u8 errcount = 0;
	//定义要发送的信息
	_mbdata_st mbp_s;
	_mbdata_st	mbp_r; 
	mbp_s.addr = 0x03;
	mbp_s.start = 0x0000;
	mbp_s.len = 0x01;//如果是0x06则此处意义不是读取寄存器个数
	
	mbp_r.addr = mbp_s.addr;
	mbp_r.start = mbp_s.start;
	mb_sent_writeHoldingReg_all(mbp_s, 0x03);
	delay_ms(10);//进入中断函数，防止直接进入while循环
	while(mb_recv_readHoldingReg_all(&mbp_r,0x03) != res_OK)
	{
		delay_ms(20);
		mb_sent_writeHoldingReg_all(mbp_s, 0x03);
		errcount++;
		if(errcount>20)  {errcount = 0; printf("WindsDirection error!\r\n"); break;}
	}//接收到数据并保存在mbp_r中,若错误则间隔20ms重复发送
//	printf("raw data:%d\r\n", mbp_r.buf[0]);
	return mbp_r.buf[0];//返回的风速值扩大了十倍，保留一位小数
}

 /*
修改当前地址，functioncode 0x06
参数：Previousadd:修改之前设备地址  Currentadd：修改之后设备地址
返回值：1清除成功 0清除失败
*/

bool ModifyAddress(u8 Previousadd, u8 Currentadd)
{
	u8 errcount = 0;
	//定义要发送的信息
	_mbdata_st mbp_s;
	_mbdata_st	mbp_r; 
	mbp_s.addr = Previousadd;
	mbp_s.start = 0x07D0;
	mbp_s.len = Currentadd;//修改的新地址值，此处修改从机地址为02
	
	mbp_r.addr = mbp_s.addr;
	mbp_r.start = mbp_s.start;
	mb_sent_writeHoldingReg_all(mbp_s, 0x06);
	delay_ms(10);//进入中断函数，防止直接进入while循环
	while(mb_recv_readHoldingReg_all(&mbp_r,0x06) != res_OK)
	{
		delay_ms(20);
		mb_sent_writeHoldingReg_all(mbp_s, 0x06);
		errcount++;
		if(errcount>50) {errcount = 0; return 0;}
	}//接收到数据并保存在mbp_r中,若错误则间隔20ms重复发送
	return 1;
}



/*
查询当前地址，functioncode 0x03 
返回值为 当前地址值
*/
void CurrentAddress(void)
{
	u8 errcount = 0;
	//定义要发送的信息
	_mbdata_st mbp_s;
	_mbdata_st	mbp_r; 
	mbp_s.addr = 0xFF;
	mbp_s.start = 0x07D0;
	mbp_s.len = 0x01;//如果是0x06则此处意义不是读取寄存器个数
	
	mbp_r.addr = mbp_s.addr;
	mbp_r.start = mbp_s.start;
	mb_sent_writeHoldingReg_all(mbp_s, 0x03);
	delay_ms(10);//进入中断函数，防止直接进入while循环
	while(mb_recv_readHoldingReg_all(&mbp_r,0x03) != res_OK)
	{
		delay_ms(20);
		mb_sent_writeHoldingReg(mbp_s);
		errcount++;
		if(errcount>50) {errcount = 0; break;}
	}//接收到数据并保存在mbp_r中,若错误则间隔20ms重复发送
//	printf("Address:%d\r\n", mbp_r.buf[0]);
	//return mbp_r.buf[0]/10;//返回地址
}	



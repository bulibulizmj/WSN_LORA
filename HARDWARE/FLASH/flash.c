#include "flash.h"

#include<string.h>
uint16_t write_data[DATA_FLASH_SAVE_NUM];
uint16_t read_data[DATA_FLASH_SAVE_NUM];

//通过地址获取扇区位置
uint16_t STMFLASH_GetFlashSector(u32 addr)
{
	if(addr<ADDR_FLASH_SECTOR_1)return FLASH_Sector_0;
	else if(addr<ADDR_FLASH_SECTOR_2)return FLASH_Sector_1;
	else if(addr<ADDR_FLASH_SECTOR_3)return FLASH_Sector_2;
	else if(addr<ADDR_FLASH_SECTOR_4)return FLASH_Sector_3;
	else if(addr<ADDR_FLASH_SECTOR_5)return FLASH_Sector_4;
	else if(addr<ADDR_FLASH_SECTOR_6)return FLASH_Sector_5;
	else if(addr<ADDR_FLASH_SECTOR_7)return FLASH_Sector_6;
	else if(addr<ADDR_FLASH_SECTOR_8)return FLASH_Sector_7;
	else if(addr<ADDR_FLASH_SECTOR_9)return FLASH_Sector_8;
	else if(addr<ADDR_FLASH_SECTOR_10)return FLASH_Sector_9;
	else if(addr<ADDR_FLASH_SECTOR_11)return FLASH_Sector_10; 
	return FLASH_Sector_11;	
}

//将数据写入内存 16位数据
int write_flash(uint16_t *FlashWriteBuf)
{
	int i = 0;
	uint32_t StartAddr;
	StartAddr = FLASH_SAVE_ADDR;

	FLASH_Unlock();	//解锁
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                  FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR|FLASH_FLAG_PGSERR);

	if (FLASH_COMPLETE != FLASH_EraseSector(STMFLASH_GetFlashSector(StartAddr),VoltageRange_2)) //擦除扇区内容
    {		
		return TEST_ERROR;
	}
	
	for ( i = 0; i < DATA_FLASH_SAVE_NUM; i++)
	{
		if (FLASH_COMPLETE != FLASH_ProgramHalfWord(StartAddr, FlashWriteBuf[i]))	//写入16位数据
		{			
			return TEST_ERROR;
		}
		StartAddr += 2;	//16位数据偏移两个位置
	}

	FLASH_Lock();	//上锁
     
	return TEST_SUCCESS;
}

//从内存读数据 16位数据
void read_flash(uint16_t *FlashReadBuf)
{	
	int i = 0;
	uint32_t StartAddr = FLASH_SAVE_ADDR;
	for (i = 0; i < DATA_FLASH_SAVE_NUM; i++)
	{
		FlashReadBuf[i] = *(__IO uint16_t*)StartAddr;
		StartAddr += 2;
	}
}

void write_to_flash(uint64_t data)
{
	memset(write_data, 0, sizeof(write_data));
	/*
		//这里就可以写入一些参数 如kp、ki
		write_data[0] = kp;
		write_data[1] = ki;
	*/
	write_data[0] = data;
  write_data[1] = data >> 16;
  write_data[2] = data >> 32;
  write_data[3] = data >> 48;
	if(TEST_SUCCESS!=write_flash(write_data))	
		return;  //写入错误

}


uint64_t read_from_flash(void)
{
	memset(read_data, 0, sizeof(read_data));
	read_flash(read_data);
	/*
		//这里读取数据
		kp = read_data[0];
		ki = read_data[1];
	*/
  return read_data[0] + ((uint64_t)read_data[1] << 16) + ((uint64_t)read_data[2] << 32) + ((uint64_t)read_data[3] << 48);
  
}
/*
	读取第一个16位数据并返回
*/
u16 read_from_flash_16bit(void)
{
	memset(read_data, 0, sizeof(read_data));
	read_flash(read_data);
	return read_data[0];
}


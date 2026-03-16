
#include "mac.h"
#include "freertos_demo.h"
#include "FreeRTOS.h"
#include "event_groups.h" 
#include "task.h"
#include <math.h>
#include <string.h>
#include "Mb_usart.h"
extern u8 lora_serialRXbuf_st[LORA_SERIAL_BUF_SIZE];				//串口接收缓存
extern u16 lora_Rxcouter;									//串口接收字节个数
u8 lora_serialTXbuf_st[LORA_SERIAL_BUF_SIZE];							//串口发送缓存
MACframe recv_frame;											//接收数据帧
MACframe send_frame;							        //发送数据帧

NodeAddr ADDR_MINE;                       //节点MAC层地址，由节点节点地理坐标来定义，在路由层函数中写入
NodeAddr ADDR_CURRENT = 0;								//当前通信的节点地址
u8 recv_rssi = 0;

//EventGroupHandle_t channel_tab_handle;		//
//EventBits_t channel_tab_bit = 0;					//记录被占用的信道

EventGroupHandle_t recv_eventgroup_handle;
EventBits_t recv_eventgroup_bit = 0;	
extern TimerHandle_t wait_comm_timer_handle; 			/* 单次定时器 */
extern TimerHandle_t reset_recv_timer_handle;
extern TaskHandle_t mac_packet_process_handler;

/* ------------------------ CSMA/CA congestion statistics ------------------------
 * We treat each CSMA listen window as one sample:
 * - busy=1 if we received any byte during the window (CSMA_BUSY_7 set)
 * - busy=0 if the window timed out without receiving bytes
 * The congestion score is the busy-hit ratio in a sliding window.
 */
#if MAC_CSMA_CONGESTION_WINDOW_SIZE > 0
static u8 g_csma_busy_window[MAC_CSMA_CONGESTION_WINDOW_SIZE];
static u16 g_csma_busy_hits = 0;
static u16 g_csma_window_count = 0;
static u16 g_csma_window_index = 0;
#endif

static void MAC_CSMA_RecordListenWindow(u8 busy)
{
#if MAC_CSMA_CONGESTION_WINDOW_SIZE == 0
    (void)busy;
#else
    u8 old = 0;

    busy = (busy != 0) ? 1 : 0;

    taskENTER_CRITICAL();
    if (g_csma_window_count < (u16)MAC_CSMA_CONGESTION_WINDOW_SIZE)
    {
        g_csma_busy_window[g_csma_window_index] = busy;
        if (busy)
        {
            g_csma_busy_hits++;
        }
        g_csma_window_count++;
    }
    else
    {
        old = g_csma_busy_window[g_csma_window_index];
        if (old)
        {
            g_csma_busy_hits--;
        }

        g_csma_busy_window[g_csma_window_index] = busy;
        if (busy)
        {
            g_csma_busy_hits++;
        }
    }

    g_csma_window_index++;
    if (g_csma_window_index >= (u16)MAC_CSMA_CONGESTION_WINDOW_SIZE)
    {
        g_csma_window_index = 0;
    }
    taskEXIT_CRITICAL();
#endif
}

float MAC_GetCsmaCongestionScore(void)
{
#if MAC_CSMA_CONGESTION_WINDOW_SIZE == 0
    return 0.0f;
#else
    float score = 0.0f;

    taskENTER_CRITICAL();
    if (g_csma_window_count > 0)
    {
        score = (float)g_csma_busy_hits / (float)g_csma_window_count;
    }
    taskEXIT_CRITICAL();

    if (score < 0.0f)
    {
        score = 0.0f;
    }
    if (score > 1.0f)
    {
        score = 1.0f;
    }

    return score;
#endif
}
/**
  * @brief  MACFrame帧发送函数，将MAC帧赋值到数组中
  * @param  None
  * @retval None
  */
void lora_Send_MACFrame(MACframe* macframe)    
{
    u16 crc = 0;
    u16 i = 0;
    u16 body_len = 0;
    u8 data_len = 0;
    u16 total_len = 0;

    if (macframe == NULL)
    {
        return;
    }

    memset(lora_serialTXbuf_st, 0, sizeof(lora_serialTXbuf_st)); //清空串口发送数组

    /* 控制帧裁剪：RTS/CTS/ACK 只发送 frame_type + src + dst */
    if ((macframe->frame_type == 1) || (macframe->frame_type == 2) || (macframe->frame_type == 3))
    {
        body_len = (u16)(1u + (u16)sizeof(NodeAddr) + (u16)sizeof(NodeAddr));
    }
    else
    {
        /* 数据帧保持兼容：发送MACframe除checksum外的全部字段 */
        body_len = (u16)(sizeof(MACframe) - 2u);
    }

    data_len = (u8)(body_len + 2u); /* LEN字段包含CRC16 */
    if ((data_len < (u8)LORA_FRAME_DATA_MIN_LEN) || (data_len > (u8)LORA_FRAME_DATA_MAX_LEN))
    {
        return;
    }

    lora_serialTXbuf_st[0] = (u8)LORA_FRAME_SOF0;
    lora_serialTXbuf_st[1] = (u8)LORA_FRAME_SOF1;
    lora_serialTXbuf_st[2] = data_len;

    memcpy(&lora_serialTXbuf_st[LORA_FRAME_HDR_LEN], macframe, body_len);
    crc = mc_check_crc16(&lora_serialTXbuf_st[LORA_FRAME_HDR_LEN], body_len);
    lora_serialTXbuf_st[LORA_FRAME_HDR_LEN + body_len] = (u8)(crc >> 8);
    lora_serialTXbuf_st[LORA_FRAME_HDR_LEN + body_len + 1] = (u8)crc;

    total_len = (u16)(LORA_FRAME_HDR_LEN + (u16)data_len);
    printf("要发送的MAC帧为：\r\n");
    for (i = 0; i < total_len; i++)
    {
        printf("%.2x ", lora_serialTXbuf_st[i]);
    }
    printf("\r\n");

    myUSART_Sendarr(UART4, lora_serialTXbuf_st, (u8)total_len);
}


/**
  * @brief  STM32F4 自带了硬件随机数发生器（RNG），
	* 				RNG 处理器是一个以连续模拟噪声为基础的随机数发生器，在主机读数时提供一个32 位的随机数
  * @param  None
  * @retval 0:初始化成功，1初始化失败
  */
bool RNG_Init(void)
{
		u16 i = 0;
		RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG,ENABLE); //使能RNG时钟，在AHB2总线上
		RNG_Cmd(ENABLE);//使能RNG
		while(RNG_GetFlagStatus(RNG_FLAG_DRDY)==0)  //等待DRDY稳定，稳定之后不为0，返回1
		{
				i++;
				delay_us(100);
				if(i >= 10000)
				{
						return 1;	//超时强制返回
				}
		}
		return 0;
}

/**
  * @brief  读取数值函数
  * @param  None
  * @retval 获取的随机数
  */
u32 RNG_Get_RandomNum(void)
{
		if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
		{
				TickType_t start = xTaskGetTickCount();
				while(RNG_GetFlagStatus(RNG_FLAG_DRDY)==0)  //等待稳定
				{
						// 防止硬件RNG异常导致死循环卡死系统
						if((xTaskGetTickCount() - start) > pdMS_TO_TICKS(10))
						{
								return (u32)xTaskGetTickCount();
						}
						vTaskDelay(1);
				}
		}
		else
		{
				u32 i = 0;
				while(RNG_GetFlagStatus(RNG_FLAG_DRDY)==0)  //等待稳定
				{
						delay_us(10);
						if(++i >= 100000)
						{
								return 0;
						}
				}
		}
		return RNG_GetRandomNumber();   //获取并返回数值
}

/**
  * @brief  获取指定范围[min, max]的随机数
  * @param  None
  * @retval 获取指定范围的随机数
  */
int RNG_Get_RandnomRange(int min, int max)
{
		return min +  RNG_Get_RandomNum()%(max-min+1);  //使数据位于某个范围
}



/**
  * @brief  载波侦听，采用CSMA/CA算法
  * @param  None
  * @retval 1：信道空闲，侦听成功；0：信道非空闲，侦听失败
  */
bool is_channel_idle(void)								
{
		u8 NB = 0, CW = 1, BE = 0;//NB：监听次数，初始值为0;CW：在传输数据前需要连续侦测的频道空闲次数;BE：决定随机延迟时间的大小
		u8 macBE[3] = {1, 2, 3};	//BE的取值范围
//    u8 macBE[3] = {2, 3, 4};	//BE的取值范围
		u8 backoff = 0, cnt = 0;
		RNG_Init();
		while(1)
		{
        recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, IS_SENDER | IS_RECEIVER | WAIT_FOR_COMM, pdFALSE, pdTRUE, portMAX_DELAY);//既不为发送者也不为接收者且不在强制休眠时，才可以发送数据
				xEventGroupClearBits(recv_eventgroup_handle, CSMA_BUSY_7);//重置CSMA接收的事件标志位
				BE = macBE[cnt];
//        printf("pow(2,BE)= %lf\r\n", pow(2,BE));//会触发硬件错误
				backoff = RNG_Get_RandnomRange(1, (int)pow(2,BE)-1);//延迟的时间 = backoff*4 秒
//        backoff = RNG_Get_RandnomRange(1, pow(2,BE)-1);//延迟的时间 = backoff*4 秒
				printf("CSMA随机时延：%d秒\r\n", 4 * backoff);
				recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, CSMA_BUSY_7, pdTRUE, pdTRUE, 4000 * backoff);
				if((recv_eventgroup_bit & CSMA_BUSY_7) == 0)	//没有获取到事件标志组
				{
            MAC_CSMA_RecordListenWindow(0);
						CW --;
						if(CW == 0) return 1;
				}
				else	//获取到了事件标志组
				{
            MAC_CSMA_RecordListenWindow(1);
						NB ++;
						cnt ++;
						CW = 1;
						if(cnt >= 2) cnt = 2;
						if(NB > 0) return 0;//最多监听1次
				}
		}
}

/**
  * @brief  RTS发送函数
  * @param  None
  * @retval 1：发送成功；0：发送失败
  */
void rts_send(NodeAddr src_addr, NodeAddr dst_addr)
{
		MACframe rts_frame;
    mac_frame_clear(&rts_frame);
		rts_frame.frame_type = 2;
		rts_frame.src_addr = src_addr;
		rts_frame.dst_addr = dst_addr;
//    printf("RTS长度为%d\r\n", sizeof(rts_frame));
		lora_Send_MACFrame(&rts_frame);		
}

/**
  * @brief  CTS发送函数,看看要不要负载清0
  * @param  None
  * @retval 1：发送成功；0：发送失败
  */
void cts_send(NodeAddr src_addr, NodeAddr dst_addr)
{
		MACframe cts_frame;
    mac_frame_clear(&cts_frame);
		cts_frame.frame_type = 3;
		cts_frame.src_addr = src_addr;
		cts_frame.dst_addr = dst_addr;
		lora_Send_MACFrame(&cts_frame);		
}

/**
  * @brief  ACK发送函数
  * @param  None
  * @retval 1：发送成功；0：发送失败
  */
void ack_send(NodeAddr src_addr, NodeAddr dst_addr)
{
		MACframe ack_frame;
    mac_frame_clear(&ack_frame);
		ack_frame.frame_type = 1;
		ack_frame.src_addr = src_addr;
		ack_frame.dst_addr = dst_addr;
		lora_Send_MACFrame(&ack_frame);		
}
/**
  * @brief  广播发送广播数据型mac帧的mac发送函数，内部包括载波侦听、真实数据帧发送,并且广播发送时，发送方地址全为F
  * @param  mac_frame：要发送的MAC帧
  * @retval 
  */
void mac_send_broadcast(void)								
{
    MACframe send_mac_frame;
    mac_frame_clear(&send_mac_frame);
    memcpy(&send_mac_frame, &send_frame, sizeof(MACframe)-2);
		while(!is_channel_idle()) 
		{
				printf("信道不空闲，CSMA失败\r\n");	//如果CSMA失败则会一直CSMA，在这期间如果接收到其他数据包则会优先处理其他数据包,但是mac_send函数不能被重复使用
		}
    xEventGroupClearBits(recv_eventgroup_handle, IS_SENDER);//清0，表示自己成为发送方
		printf("信道空闲,成为发送方\r\n");
    printf("发送广播型数据帧\r\n");
    lora_Send_DATA_MACFrame(&send_mac_frame, WITHOUT_DATA_RECV);
    xEventGroupSetBits(recv_eventgroup_handle, IS_SENDER); //IS_SENDER置1，表示不再是发送方
}

/**
  * @brief  不需要接收方发送数据型mac帧的mac发送函数，内部包括载波侦听、RTS发送，真实数据帧发送、CTS发送
  * @param  mac_frame：要发送的MAC帧
  * @retval 0：未收到CTS； 1：未收到ACK; 2: 成功发送
  */
u8 mac_send_without_data_recv(void)								
{
    MACframe send_mac_frame;
		u8 err = 0;
    mac_frame_clear(&send_mac_frame);
    memcpy(&send_mac_frame, &send_frame, sizeof(MACframe)-2);
    
		while(!is_channel_idle()) 
		{
				printf("信道不空闲，CSMA失败\r\n");	//如果CSMA失败则会一直CSMA，在这期间如果接收到其他数据包则会优先处理其他数据包,但是mac_send函数不能被重复使用
		}
    xEventGroupClearBits(recv_eventgroup_handle, IS_SENDER);//清0，表示自己成为发送方
		printf("信道空闲,成为发送方\r\n");
		ADDR_CURRENT = send_mac_frame.dst_addr;	//赋值ADDR_CURRENT
    
		while(!(recv_eventgroup_bit&CTS_RECV_0))//如果没有获取CTS，则重发RTS
		{
				
				printf("发送RTS\r\n");
				err++;
				if(err > 3)
				{
						printf("未收到CTS，重新选择父节点\r\n");	//若未获取成功则重新发送，这里可用while循环，若一直未获取CTS则考虑删除目的节点
            xEventGroupSetBits(recv_eventgroup_handle, IS_SENDER); //IS_SENDER置1，表示不再是发送方
            ADDR_CURRENT = 0;
						return 0;
				}
				rts_send(send_mac_frame.src_addr, send_mac_frame.dst_addr);
				xEventGroupSetBits(recv_eventgroup_handle, RTS_SEND);
//        recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, CTS_RECV_0, pdTRUE, pdTRUE, 5000);	//获取CTS事件标志组,调试用
				recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, CTS_RECV_0, pdTRUE, pdTRUE, 10000);	//获取CTS事件标志组
		}
		err = 0;
		while(!(recv_eventgroup_bit&ACK_RECV_5))//如果没有获取ACK，则重发数据帧
		{
				
        printf("发送数据帧\r\n");
				err++;
				if(err > 3)
				{
						printf("未收到ACK\r\n");	//若未获取成功则重新发送
            xEventGroupSetBits(recv_eventgroup_handle, IS_SENDER); //IS_SENDER置1，表示不再是发送方
            ADDR_CURRENT = 0;
						return 1;
				}
				lora_Send_DATA_MACFrame(&send_mac_frame, WITHOUT_DATA_RECV);
				xEventGroupSetBits(recv_eventgroup_handle, DATA_FRAME_SEND);
				recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, ACK_RECV_5, pdTRUE, pdTRUE, 10000);	//获取ACK事件标志组
		}
    xEventGroupSetBits(recv_eventgroup_handle, IS_SENDER); //IS_SENDER置1，表示不再是发送方
    ADDR_CURRENT = 0;
		return 2;
}

/**
  * @brief  发送数据型MAC帧函数
  * @param  frame： 要发送的数据型MAC帧
  *         config：REQUIRE_DATA_RECV：mac发送数据型MAC帧后需要接收方回复数据帧  
                    WITHOUT_DATA_RECV：mac发送数据型MAC帧后不需要接收方回复数据帧，需要接收方发送ACK  
  * @retval None
  */
void lora_Send_DATA_MACFrame(MACframe* frame, mac_send_config config)
{
    switch(config)
    {
        case REQUIRE_DATA_RECV :
            frame->require_ack = 0;
            frame->frame_type = 0;
            lora_Send_MACFrame(frame);
            break;
        case WITHOUT_DATA_RECV :
            frame->require_ack = 1;
            frame->frame_type = 0;
            lora_Send_MACFrame(frame);
            break; 
        default :
        {
            printf("发送函数配置错误，数据型MAC发送失败\r\n");
        }
    }
}
/**
  * @brief  需要接收方发送数据型mac帧的mac发送函数，内部包括载波侦听、RTS发送，真实数据帧发送、CTS发送
  * @param  mac_frame：要发送的MAC帧
  * @retval 0：未收到CTS； 1：未收到ACK; 2: 成功发送
  */
u8 mac_send_require_data_recv(void)								
{
    MACframe send_mac_frame;
		u8 err = 0;
    mac_frame_clear(&send_mac_frame);
    memcpy(&send_mac_frame, &send_frame, sizeof(MACframe)-2);
    xEventGroupClearBits(recv_eventgroup_handle, DATA_FRAME_RECV);
		while(!is_channel_idle()) 
		{
				printf("信道不空闲，CSMA失败\r\n");	//如果CSMA失败则会一直CSMA，在这期间如果接收到其他数据包则会优先处理其他数据包,但是mac_send函数不能被重复使用
		}
    xEventGroupClearBits(recv_eventgroup_handle, IS_SENDER);//清0，表示自己成为发送方
		printf("信道空闲,成为发送方\r\n");
		ADDR_CURRENT = send_mac_frame.dst_addr;	//赋值ADDR_CURRENT
    
		while(!(recv_eventgroup_bit&CTS_RECV_0))//如果没有获取CTS，则重发RTS
		{
				
				printf("发送RTS\r\n");
				err++;
				if(err > 3)
				{
						printf("未收到CTS，重新选择父节点\r\n");	//若未获取成功则重新发送，这里可用while循环，若一直未获取CTS则考虑删除目的节点
            xEventGroupSetBits(recv_eventgroup_handle, IS_SENDER); //IS_SENDER置1，表示不再是发送方
            ADDR_CURRENT = 0;
						return 0;
				}
				rts_send(send_mac_frame.src_addr, send_mac_frame.dst_addr);
				xEventGroupSetBits(recv_eventgroup_handle, RTS_SEND);
//        recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, CTS_RECV_0, pdTRUE, pdTRUE, 5000);	//获取CTS事件标志组,调试用
				recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, CTS_RECV_0, pdTRUE, pdTRUE, 10000);	//获取CTS事件标志组
		}
    
		err = 0;
		while(!(recv_eventgroup_bit&DATA_FRAME_RECV))//如果没有获取DATA，则重发数据帧
		{
				
        printf("发送数据帧\r\n");
				err++;
				if(err > 3)
				{
						printf("未收到DATA frame\r\n");	//若未获取成功则重新发送
            xEventGroupSetBits(recv_eventgroup_handle, IS_SENDER); //IS_SENDER置1，表示不再是发送方
            ADDR_CURRENT = 0;
						return 0;
				}
				lora_Send_DATA_MACFrame(&send_mac_frame, REQUIRE_DATA_RECV);
				xEventGroupSetBits(recv_eventgroup_handle, DATA_FRAME_SEND);
				recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, DATA_FRAME_RECV, pdTRUE, pdTRUE, 10000);	//获取DATA事件标志组
		}
    
   	err = 0; 
    while(!(recv_eventgroup_bit&ACK_RECV_5))//如果没有获取ACK，则重发数据帧(该数据帧其实是负载为ACK的包)
		{
				
        printf("发送数据帧\r\n");
				err++;
				if(err > 3)
				{
						printf("未收到ACK\r\n");	//若未获取成功则重新发送
            xEventGroupSetBits(recv_eventgroup_handle, IS_SENDER); //IS_SENDER置1，表示不再是发送方
            ADDR_CURRENT = 0;
						return 1;
				}
        mac_frame_clear(&send_mac_frame);
        memcpy(&send_mac_frame, &send_frame, sizeof(MACframe)-2);//在这步时send_frame已经在路由层中被改变成为ACK route帧了
				lora_Send_DATA_MACFrame(&send_mac_frame, WITHOUT_DATA_RECV);
				xEventGroupSetBits(recv_eventgroup_handle, DATA_FRAME_SEND);
				recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, ACK_RECV_5, pdTRUE, pdTRUE, 10000);	//获取ACK事件标志组
        printf("接收到ACK或超时\r\n");
		}

    xEventGroupSetBits(recv_eventgroup_handle, IS_SENDER); //IS_SENDER置1，表示不再是发送方
    ADDR_CURRENT = 0;
		return 2;
}

/**
  * @brief  mac发送函数
  * @param  mac_frame：要发送的MAC帧
  *         config   ：REQUIRE_DATA_RECV：mac发送需要接收方回复数据帧  
                       WITHOUT_DATA_RECV：mac发送不需要接收方回复数据帧  
  * @retval 0：未收到CTS； 1：未收到ACK; 2: 成功发送； 4：函数配置错误
  */
u8 mac_send(mac_send_config config)
{
    u8 ret = 4;
    switch(config)
    {
        case REQUIRE_DATA_RECV :
            ret = mac_send_require_data_recv();
            break;
        case WITHOUT_DATA_RECV :
            ret = mac_send_without_data_recv();
            break; 
        default :
        {
            printf("发送函数配置错误，MAC发送失败\r\n");
        }
    }
    return ret;
}

/**
  * @brief  数据接收函数,将LoRa接收到的数据帧转为MAC数据帧
	*													 
  * @param  None
  * @retval None
  */
void packet_receive_mac(void)
{
    u16 crc_check = 0;
    u16 crc_recv = 0;
    u16 i = 0;
    u8 data_len = 0;
    u16 body_len = 0;
    u16 copy_len = 0;
    const u16 data_offset = (u16)(LORA_FRAME_RSSI_LEN + LORA_FRAME_HDR_LEN);

    for (i = 0; i < lora_Rxcouter; i++)
    {
        printf("%.2x ", lora_serialRXbuf_st[i]);
    }
    printf("\r\n");

    if (lora_Rxcouter < (u16)(LORA_FRAME_RSSI_LEN + LORA_FRAME_HDR_LEN + LORA_FRAME_DATA_MIN_LEN))
    {
        return;
    }

    /* Frame header check */
    if ((lora_serialRXbuf_st[1] != (u8)LORA_FRAME_SOF0) || (lora_serialRXbuf_st[2] != (u8)LORA_FRAME_SOF1))
    {
        return;
    }

    recv_rssi = lora_serialRXbuf_st[0];
    data_len = lora_serialRXbuf_st[3];
    if ((data_len < (u8)LORA_FRAME_DATA_MIN_LEN) || (data_len > (u8)LORA_FRAME_DATA_MAX_LEN))
    {
        return;
    }

    if (lora_Rxcouter != (u16)(LORA_FRAME_RSSI_LEN + LORA_FRAME_HDR_LEN + (u16)data_len))
    {
        return;
    }

    body_len = (u16)((u16)data_len - 2u);
    if (body_len == 0)
    {
        return;
    }

    crc_check = mc_check_crc16(&lora_serialRXbuf_st[data_offset], body_len);
    crc_recv = (u16)(((u16)lora_serialRXbuf_st[data_offset + body_len] << 8) |
                     (u16)lora_serialRXbuf_st[data_offset + body_len + 1]);
    if (crc_check == crc_recv)
    {
        xEventGroupSetBits(recv_eventgroup_handle, CRC_CHECK_3);

        mac_frame_clear(&recv_frame);
        copy_len = body_len;
        if (copy_len > (u16)(sizeof(MACframe) - 2u))
        {
            copy_len = (u16)(sizeof(MACframe) - 2u);
        }
        memcpy(&recv_frame, &lora_serialRXbuf_st[data_offset], copy_len);
        recv_frame.checksum = crc_recv;
    }
}

/**
  * @brief  清空mac帧
	*													 
  * @param  None
  * @retval 
  */
void mac_frame_clear(MACframe* macframe)
{
		memset(macframe, 0, sizeof(MACframe));//清空结构体成员数据
}


/**
  * @brief  数据处理函数，会对lora接收到的不同的数据进行不同的处理,
	*					如果接收到的是真实数据，则通知路由层的数据处理函数来处理
	*					该函数在LoRa接收到数据之前一直处于阻塞态.
	*					丢弃数据包的情形：①目的地址不是自身且自身不处于休眠态的数据包
	*													 ②源地址与当前通信的节点地址不符的数据包
  *													 ③CRC校验不通过的数据包			
  * @param  
  * @retval 
  */
void packet_process_mac(void)			
{		

		while(1)
		{
				const TickType_t wait_ticks = pdMS_TO_TICKS(1000);
				recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, PACKET_RECV_2, pdTRUE, pdTRUE, wait_ticks);
				if(!(recv_eventgroup_bit & PACKET_RECV_2))
				{
						WDG_Heartbeat(WDG_HB_ID_MAC);
						vTaskDelay(10);
						continue;
				}

				packet_receive_mac();
				recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, CRC_CHECK_3, pdTRUE, pdTRUE, 0);
				if((recv_eventgroup_bit)&CRC_CHECK_3)
				{

						// if(未入网（）)	
						// {
						// 		解析MAC帧，等待一段时间后，请求入网（）；
						// }						
						switch(recv_frame.frame_type)
						{
                case 0 :	//数据型MAC帧，接收时需判断自身是否是接收方且发送方是否是当前建立连接的节点，并且在发送无负载的ACK后，需要将IS_RECEIVER置1，ADDR_CURRENT清0
                    DATA_PACKET_PROCESS();
                    break;
                case 1 :	//ACK型MAC帧，接收到时需判断自身是否是发送方并且已经发送了数据帧，作为发送方时，收到ACK后需将IS_SENDER置1代表发送结束，有两种情况会收到ACK：一种是自身为发送方时接收到对方的ACK，另一种是接收方时发送了有负载的ACK后，需要接收发送方发送的ACK
                    ACK_PACKET_PROCESS();
                    break;
                case 2 :	//RTS型MAC帧，接收到时需判断自身是否既不是接收方也不是发送方，如果都不是才可以不丢弃这个RTS，如果RTS目的地不是当前节点则进入强制休眠，如果是则成为接收方，发送CTS
                    RTS_PACKET_PROCESS();
                    break;
                case 3 :	//CTS型MAC帧，接收到时需判断自身是否是发送方并且已经发送了RTS
                    CTS_PACKET_PROCESS();
                    break;
                default :
                    printf("无效的MAC帧\r\n");
						}
						
				}
				else
				{
//						printf("CRC校验不通过\r\n");
				}
				Clear_Buffer_LORA();
				WDG_Heartbeat(WDG_HB_ID_MAC);
        vTaskDelay(10);
		}

}

/**
  * @brief  对接收到的RTS包进行处理
  * @param  
  * @retval None
  */
void RTS_PACKET_PROCESS(void)
{
    printf("接收到RTS型MAC帧了\r\n");			
    if(recv_frame.dst_addr != ADDR_MINE)	//判断接收方是否是自己
    {
        recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, IS_SENDER | IS_RECEIVER | WAIT_FOR_COMM, pdFALSE, pdTRUE, 0);	//判断自己是否是发送方或接收方或刚接收到过其他节点的CTS或RTS，也就是说判断自己当前有没有在与其他节点通信或正在强制休眠，若是则丢弃
        if((recv_eventgroup_bit & IS_SENDER) && (recv_eventgroup_bit & IS_RECEIVER))	//当前自身既不是发送方也不是接收方，则需要多休眠一段时间
        {
            printf("接收到了别的节点的RTS\r\n");
            xEventGroupClearBits(recv_eventgroup_handle, WAIT_FOR_COMM);	//WAIT_FOR_COMM标志位清0，表示进入强制休眠状态
            xTimerStart(wait_comm_timer_handle, 0);		//开启定时器
            printf("开启定时\r\n");
            if(!(recv_eventgroup_bit & WAIT_FOR_COMM)) //如果正在进行强制休眠时接收到了其他节点的RTS，则重置定时器
                xTimerReset(wait_comm_timer_handle, portMAX_DELAY);
        }
        else
        {
            printf("丢弃\r\n");
        }
    }
    else	//接收方是自己，判断自身是否空闲，如果空闲则成为接收方，发送CTS，如果不空闲则判断自己是否已经成为了接收方，如果是则判断接收到的RTS源地址与当前通信节点的地址是否一样，一样则说明当前通信的节点没收到之前发送的CTS，所以重发
    {
        recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, IS_SENDER | IS_RECEIVER | WAIT_FOR_COMM, pdFALSE, pdTRUE, 0);
        if((recv_eventgroup_bit & IS_SENDER) && (recv_eventgroup_bit & IS_RECEIVER) && (recv_eventgroup_bit & WAIT_FOR_COMM))
        {
            printf("接收到了RTS，发送CTS\r\n");
            xEventGroupClearBits(recv_eventgroup_handle, IS_RECEIVER);	//清0，表示自身成为接收者
            ADDR_CURRENT = recv_frame.src_addr;	//赋值ADDR_CURRENT
            xTimerStart(reset_recv_timer_handle, 0);		//开启定时器
            cts_send(ADDR_MINE, recv_frame.src_addr); 
        }
        else if(!(recv_eventgroup_bit & IS_RECEIVER))//已经成为了接收方
        {
            if(ADDR_CURRENT == recv_frame.src_addr)//RTS源地址是当前通信节点
            {
                printf("重新发送CTS\r\n");
                cts_send(ADDR_MINE, recv_frame.src_addr);
            }
        }
    }
}

/**
  * @brief  对接收到的CTS包进行处理
  * @param  
  * @retval None
  */
 void CTS_PACKET_PROCESS(void)
 {
    printf("接收到CTS型MAC帧了\r\n");

    if(recv_frame.dst_addr != ADDR_MINE)	//判断接收方是否是自己
    {
        recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, IS_SENDER | IS_RECEIVER | WAIT_FOR_COMM, pdFALSE, pdTRUE, 0);	//判断自己是否是发送方或接收方或刚接收到过其他节点的CTS或RTS，也就是说判断自己当前有没有在与其他节点通信或正在强制休眠，若是则丢弃
        if((recv_eventgroup_bit & IS_SENDER) && (recv_eventgroup_bit & IS_RECEIVER))	//当前自身既不是发送方也不是接收方，则需要多休眠一段时间
        {
            printf("接收到了别的节点的CTS,强制休眠\r\n");
            xEventGroupClearBits(recv_eventgroup_handle, WAIT_FOR_COMM);	//WAIT_FOR_COMM标志位清0
            xTimerStart(wait_comm_timer_handle, 0);		//开启定时器
            printf("开启定时\r\n");
            if(!(recv_eventgroup_bit & WAIT_FOR_COMM)) //如果正在进行强制休眠时接收到了其他节点的RTS，则重置定时器
                xTimerReset(wait_comm_timer_handle, portMAX_DELAY);
        }
        else
        {
            printf("丢弃\r\n");
        }
    }
    else	//接收方是自己，判断发送方是不是当前想要通信的节点且判断RTS是否发送
    {
        if(recv_frame.src_addr == ADDR_CURRENT)
        {
            recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, RTS_SEND, pdTRUE, pdTRUE, 0);	//判断RTS是否已经发送
            if(recv_eventgroup_bit & RTS_SEND)
            {
                printf("接收到了正确的CTS\r\n");
                xEventGroupSetBits(recv_eventgroup_handle, CTS_RECV_0);	//接收CTS标志位置1
            }
        }
    }									
}

/**
  * @brief  对接收到的ACK包进行处理
  * @param  
  * @retval None
  */
 void ACK_PACKET_PROCESS(void)
 {
    printf("接收到ACK型MAC帧了\r\n");
    if((recv_frame.dst_addr == ADDR_MINE) && (recv_frame.src_addr == ADDR_CURRENT))	//如果接收方是自己
    {
        recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, DATA_FRAME_SEND, pdTRUE, pdTRUE, 0);	//判断数据帧是否已经发送
        if(recv_eventgroup_bit & DATA_FRAME_SEND)
        {
            printf("接收到了正确的ACK\r\n");
            xEventGroupSetBits(recv_eventgroup_handle, ACK_RECV_5);	//接收ACK标志位置1
//													xEventGroupSetBits(recv_eventgroup_handle, IS_SENDER) //IS_RECEIVER位置1，表示不再是发送方
        }
    }

}

/**
  * @brief  对接收到的数据类型的包进行处理
  * @param  
  * @retval None
  */ 
 void DATA_PACKET_PROCESS(void)
{
    printf("接收到数据型MAC帧了\r\n");	//通知路由层的处理函数来处理，接收时需判断自身是否是接收方且发送方是否是当前建立连接的节点
    
    recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, IS_RECEIVER, pdFALSE, pdTRUE, 0);
    if((recv_frame.dst_addr == ADDR_MINE) && (!(recv_eventgroup_bit & IS_RECEIVER))&&(recv_frame.src_addr == ADDR_CURRENT))  //当前是接收方
    {
        if(recv_frame.require_ack)  //需要ACK回复
        {
            printf("需要自身发送ACK\r\n");
            xEventGroupSetBits(recv_eventgroup_handle, ROUTE_PACKET_RECV_RECEIVCER);
            ack_send(ADDR_MINE, recv_frame.src_addr);
            xEventGroupSetBits(recv_eventgroup_handle, IS_RECEIVER); //IS_RECEIVER位置1，表示不再是接收方,如果发送方没有接收到我发送的ACK而重新发送数据类型的帧，我会直接丢弃
            ADDR_CURRENT = 0;
        }
        else
        {
            printf("需要自身发送数据型MAC帧\r\n");
            xEventGroupSetBits(recv_eventgroup_handle, ROUTE_PACKET_RECV_RECEIVCER);
        }
        return;
    }
    recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, IS_SENDER, pdFALSE, pdTRUE, 0);	//获取发送方标志位
    if((recv_frame.dst_addr == ADDR_MINE) && (recv_frame.src_addr == ADDR_CURRENT) && (!(recv_eventgroup_bit & IS_SENDER))) //当前是发送方
    {
        xEventGroupSetBits(recv_eventgroup_handle, ROUTE_PACKET_RECV_SENDER);//对该步的处理在最后必须对send_frame赋值，但不发送
        printf("当前是发送方并且接收到了正确的DATA\r\n");
        xEventGroupSetBits(recv_eventgroup_handle, DATA_FRAME_RECV);

        return;
    }
    
    xEventGroupSetBits(recv_eventgroup_handle, ROUTE_PACKET_RECV_SLEEP);  //通知路由层处理，但此时不准发送任何数据，该情形包括强制休眠态接收到数据、空闲态接收到数据型包、发送态或接收态接收到目的地不是自身的包
    //如果是别的节点发送了目的地不为自身的数据包或目的地为自身但自己当前正在强制休眠的数据包，则监听并处理。

}






#ifndef MAC_H
#define MAC_H

#include "stdio.h"	
#include "stm32f4xx_conf.h"
#include "sys.h"
#include "crc16.h"
#include "stdbool.h"
#include "usart.h"
#include "lora.h"


/*
 * MAC协议主要负责节点到节点的通信，即单跳通信
 * 以CSMA/CA为基础算法
 * 混合了CRSN MAC协议提升了频带利用率和数据吞吐量
 * 采用了RTS/CTS机制，解决隐藏主机问题
 */




#define CTS_RECV_0 			                      (1 << 0)		//CTS接收标志
#define RTS_RECV_1 		  	                    (1 << 1)		//RTS接收标志
#define PACKET_RECV_2		                      (1 << 2)		//数据包接收标志
#define CRC_CHECK_3			                      (1 << 3)		//CRC校验标志
#define ACK_RECV_5		  	                    (1 << 5)		//ACK接收标志
#define IS_SENDER                             (1 << 6)	  //自身不是发送方的标志，1不是发送方，0是发送方，需初始化为1
#define IS_RECEIVER                           (1 << 7)    //自身不是接收方的标志，1不是接收方，0是接收方，需初始化为1
#define CSMA_BUSY_7                           (1 << 8)	  //CSMA空闲标志，0表示空闲，1表示接收到了数据
#define RTS_SEND                              (1 << 9)	  //RTS发送标志
#define DATA_FRAME_SEND                       (1 << 10)   //数据帧发送标志
#define WAIT_FOR_COMM                         (1 << 11)	  //接收到别的节点的RTS/CTS后，强制休眠的标志，0表示正在强制休眠，1表示没有强制休眠或强制休眠结束，需初始化为1
#define INITIAL_OK                            (1 << 12)   //初始化完成标志
#define DATA_FRAME_RECV                       (1 << 14)   //数据型MAC帧接收标志
#define ROUTE_PACKET_RECV_SENDER              (1 << 15)   //路由数据包接收标志且当前为发送方，通知路由层处理
#define ROUTE_PACKET_RECV_RECEIVCER           (1 << 16)   //路由数据包接收标志且当前为接收方，通知路由层处理
#define ROUTE_PACKET_RECV_SLEEP               (1 << 17)   //路由数据包接收标志且当前为强制休眠态，通知路由层处理

/* 路由层和MAC层共用的节点地址，硬件唯一标识，由定位模块来定义 */
typedef uint64_t NodeAddr;

///* 串口接收数据缓存大小 */
//#define UART_RX_SIZE 0xff
#define ROUT_FRAME_LEN        56          //路由协议帧长度，也就是MAC帧payload的长度

#pragma pack(push, 1)                     //1字节对齐
// MAC层数据包直接使用NodeAddr作为源地址和目的地址
typedef struct {
		uint8_t  frame_type;									//帧类型（0=数据帧，1=ACK帧，2=RTS帧，3=CTS帧，4=广播数据帧）
    NodeAddr src_addr;										//源地址
    NodeAddr dst_addr;										//目的地址
    u8 require_ack;                       //是否需要ACK，仅数据型MAC帧设置时有效，如果require_ack为1表示当前通信结束，对方需要发送ACK，为0表示还需要对方传输数据型MAC帧
    u8 payload[ROUT_FRAME_LEN];						//上层（如路由协议或应用层）传递的数据
		u16 checksum;  												//CRC16校验和
} MACframe;


#pragma pack(pop)                         //还原之前的对齐方式

/* MAC发送方式枚举 */
typedef enum
{
  REQUIRE_DATA_RECV       = 0,      /* 需要接收方发送数据型MAC帧 */
  WITHOUT_DATA_RECV       = 1,      /* 不需要接收方发送数据型MAC帧 */
} mac_send_config;





void lora_Send_MACFrame(MACframe* macframe);  
void lora_Send_DATA_MACFrame(MACframe* frame, mac_send_config config);
bool RNG_Init(void);
u32 RNG_Get_RandomNum(void);
int RNG_Get_RandnomRange(int min, int max);

bool is_channel_idle(void);											           	//载波侦听，采用CSMA/CA算法
void rts_send(NodeAddr src_addr, NodeAddr dst_addr);			          	//RTS发送
void cts_send(NodeAddr src_addr, NodeAddr dst_addr);		          		//CTS发送
void ack_send(NodeAddr src_addr, NodeAddr dst_addr);                  //ACK发送函数
bool wait_for_ack(uint8_t dst_addr);					        	  	//等待ack
void packet_process_mac(void);										        	//数据处理函数，会对lora接收到的不同的数据进行不同的处理,如果接收到的是真实数据，则通知路由层的数据处理函数来处理
void mac_frame_clear(MACframe* macframe);							      //清空结构体缓存
void packet_receive_mac(void);										          //数据接收函数
u8 mac_send(mac_send_config config);		//mac发送api函数，内部包括载波侦听、RTS发送，真实数据帧发送、CTS发送
u8 mac_send_without_data_recv(void);
u8 mac_send_require_data_recv(void);
void mac_send_broadcast(void);
void RTS_PACKET_PROCESS(void);
void CTS_PACKET_PROCESS(void);
void ACK_PACKET_PROCESS(void);
void DATA_PACKET_PROCESS(void);
#endif




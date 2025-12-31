#ifndef ROUTING_H
#define ROUTING_H

#include "stdio.h"	
#include "stm32f4xx_conf.h"
#include "sys.h"
#include "mac.h"
#include "tree_node.h"

#define IS_GATWAY                          0          //网关标志，为1为网关（根节点），为0为其他节点


#define IS_JOIN_WAN                       (1 << 0)    //入网标志位，该位为0表示入网，为1表示未入网
#define IS_ADDR_NULL                      (1 << 1)    //本节点地址写入标志位，该位为1表示未写入地址，为0表示已经写入了地址
//#define IS_SENDER_ROUTE                   (1 << 3)    //路由层作为发送者的标志，为0表示正在作为发送者发送数据区，为1表示发送空闲
#define WRITE_ADDR_ORDER                  (1 << 2)    //串口1接收到上位机命令标志，写地址命令示例 1025027;314544 
#define TIMER_OK_4			                  (1 << 4)		//定时发送数据的定时器超时标志
#define BEACON_TASK_START                 (1 << 5)    //开启Beacon发送任务的标志，为1开启Beacon发送任务
#define BEACON_TIMER_OK                   (1 << 6)    //定时发送Beacon的定时器超时标志
#define JOIN_REQ_SEND                     (1 << 7)    //需要发送入网请求标志，为1启动入网请求任务发送
#define RELAY_TASK_START                  (1 << 8)    //开启数据转发任务标志，为1启动数据转发任务
#define UPDATE_TASK_START                 (1 << 9)    //开启路由更新发送任务标志，为1启动路由更新发送任务


#define MAX_CHILDREN  64        //最大子节点的数量
#define MAX_NEIGHBORS 4         //最大邻居节点（隐藏父节点）的数量
typedef struct {
  // 基础信息
  union{
      NodeAddr  addr;               //本节点的地址。高32位为经度地址，低32位为纬度地址
      uint32_t  long_latitude[2];
  } node_addr;

  NodeAddr       parent_addr;             // 父节点地址,网关的父节点地址为0xffffffffffffffff
  uint8_t        parent_rssi;             // 父节点信号强度
  uint8_t        tree_depth;              // 本节点到根节点的跳数（层级），用于数据包起始点发送时赋值给depth
  uint8_t        join_flag;               // 入网标志，为1为未入网状态，为0为已入网
  //子节点信息
  TreeNode*   tree_pointer;               // 以自身为根节点的树的头指针              
  uint8_t     child_count;                // 子节点数量
  
  // 潜在父节点信息（含层级）
  struct {
      NodeAddr addr;                  // 潜在父节点地址
      uint8_t  rssi;                  // 信号强度,rssi越小，信号强度越大
      uint8_t  depth;                 // 邻居的tree_depth
      uint32_t is_neighbors_alive;    // 该潜在父节点存活的标志，默认为1存活，如果在一定时间内没有接收到来自该节点的数据包则置0
  } neighbors[MAX_NEIGHBORS];         // 每次更新该邻居节点列表后都要进入判定当前父节点是否可以更换
  
  uint8_t neighbor_count; // 邻居数量
  
} RoutingTable;



#pragma pack(push, 1)                     //1字节对齐

// 路由协议数据帧格式（总长度建议 ≤ LoRa MTU，如255字节）
typedef struct {
  // 帧头
  uint8_t   route_frame_type;  // 帧类型 0:拒绝入网或转发；1:入网请求；2：入网回复；3：路由更新；4：上报传感器数据；5：路由层应答；6：邀请节点入网Beacon(广播，不发送RTS/CTS)   46都可以视为邀请入网信号
  uint8_t   depth;             // 当前深度
  //uint8_t   hop_limit;         // 最大剩余跳数（防环），路由更新上报、上报传感器数据时有效
  // 路由扩展字段（可选）
  //uint8_t   parent_hint;    // 建议的父节点地址（用于动态路由）
  //uint8_t   path[10];       // 源路由路径（按需填充，适用于下行数据）
  
  // 负载
  union {
      struct{
      NodeAddr  addr_src;          // 源节点地址
      uint16_t  temperature;
      uint16_t  humidity;
      uint32_t  pressure;
      uint32_t  soilstate1;
      uint32_t  soilstate2;
      uint32_t  soilstate3;
      uint16_t  precipitation;
      uint16_t  windspeed;
      uint16_t  wind_direction;
      uint16_t  radiation;
      } routing_sensor_data;
      struct {
          uint8_t  control_code; // 路由更新有效，为0表示子节点的删除，为1表示子节点的添加
          NodeAddr father;
          NodeAddr child;
      } routing_control;
  } payload;
} RoutingFrame;

#pragma pack(pop)                         //还原之前的对齐方式

void RoutingTableInitial(RoutingTable routing_table);
void write_my_addr_route(void);
void data_report_route(void);
void beacon_send_route(void);
void packet_process_route(void);
void join_wan_route(u8 *number_neighbor, u8 *rssi_neighbor, TreeNode* result);
void net_access_req(NodeAddr newfather_addr);
void packet_receive_route(void);
void routing_frame_clear(RoutingFrame* routing_frame);
void add_neighbor_route(NodeAddr addr, uint8_t rssi, uint8_t depth);
u8 search_neighbor_route(NodeAddr addr);
void search_max_rssi_neighbor_route(u8* number, u8* rssi);
void search_min_rssi_neighbor_route(u8* number, u8* rssi);
void delete_neighbor_route(NodeAddr addr_neighbor);
void data_relay_route(void);
void node_check_route(void);
void delete_dead_neighbor_route(void);
void route_update_process(void);
void update_broadcast_route(void);
void SensorDataGet(void);
void MqttReport(RoutingFrame report_data_frame);
void MqttConnect(void);
#endif




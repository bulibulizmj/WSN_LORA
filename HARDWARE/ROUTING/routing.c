
#include "routing.h" 
#include "freertos_demo.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "event_groups.h" 
#include "task.h"
#include "flash.h"
#include <string.h>
#include "sht45.h"
#include "myiic.h"
#include "bmp280.h"
#include "ec20.h"
#include "mdbs_func.h"
#include "led.h"

//阿里云物联网平台需要介入的参数产品秘钥 设备名称 设备秘钥
#define PRODUCTKEY "k0puwvMslxK"
#define DEVICENAME   "test01"
#define DEVICESECRET   "20bb28caf9e9ed27ce79b9a66cfc2f20"

extern NodeAddr ADDR_MINE;                                  //节点MAC层地址，由节点节点地理坐标来定义
extern NodeAddr ADDR_CURRENT;										            //当前通信的节点地址
extern MACframe send_frame;							                    //发送MAC数据帧缓存，在路由层中仅对send_frame.src_addr和对send_frame.dst_addr以及payload进行操作
extern MACframe recv_frame;                                 //接收MAC数据帧缓存

extern EventGroupHandle_t recv_eventgroup_handle;		  //MAC层接收事件标志组句柄
extern EventBits_t recv_eventgroup_bit;

RoutingFrame send_frame_route;                //发送路由帧缓存
RoutingFrame recv_frame_route;                //接收路由帧缓存
RoutingFrame relay_frame_route;               //转发路由帧缓存
RoutingFrame update_frame_route;              //路由更新路由帧缓存
RoutingFrame send_in_recv_frame_route;        //接收时发送的路由帧缓存
extern u8  recv_rssi;

char send_data_4g[BUFLEN];  //4G模块发送缓存

extern TimerHandle_t send_timer_handle;				  /* 单次定时器 */
extern TimerHandle_t beacon_send_timer_handle;  /* 单次定时器 */
extern TaskHandle_t write_my_addr_handler;      //任务句柄

EventGroupHandle_t route_eventgroup_handle;		//路由层事件标志组句柄
EventBits_t route_eventgroup_bit;

QueueHandle_t is_sender_route_handle;  //路由层作为发送者的标志，为0表示正在作为发送者发送数据区，为1表示发送空闲
//如果接收到的数据包的源节点不是自己的子节点且该数据包也不是入网请求，那么会直接将该节点加入到自己的子节点列表中
//同理，如果接收到的数据包的源节点是自己的子节点但目的地不是自己，则会直接将该节点从自己的子节点列表中删除


RoutingTable routing_table; //定义路由表

/**
  * @brief  初始化路由表
  * @param  None
  * @retval None
  */
void RoutingTableInitial(RoutingTable table)
{
    table.tree_pointer = NULL;
    table.node_addr.addr = read_from_flash();
    table.parent_addr = NULL;
    table.parent_rssi = 0xff;
    table.child_count = 0;
    table.neighbor_count = 0;
    table.join_flag = 1;
    table.tree_depth = 0xff;
}


/**
  * @brief  地址写入函数
  * @param  None
  * @retval None
  */
void write_my_addr_route(void)
{
    u8 i = 0;
    while(1)
    {
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_ADDR_NULL | WRITE_ADDR_ORDER, pdTRUE, pdFALSE, portMAX_DELAY);
        while((routing_table.node_addr.addr == NULL) || (routing_table.node_addr.addr == 0xffffffffffffffff)) //当前没有写入过地址
        {
            printf("请输入经纬度，默认北纬东经，先经度后纬度，示例：1025027;314544\r\n");
            delay_xms(2000);
            if(USART_RX_STA != 0)
            {
                routing_table.node_addr.long_latitude[0] = 0;
                routing_table.node_addr.long_latitude[1] = 0;
                i = 0;
                while ((USART_RX_BUF[i] != ';') && (USART_RX_BUF[i] != '\0'))
                {
                    if((USART_RX_BUF[i] >= '0') && (USART_RX_BUF[i] <= '9'))
                    {    
                        routing_table.node_addr.long_latitude[0] = routing_table.node_addr.long_latitude[0]*10 + USART_RX_BUF[i] - '0';
                        printf("%d", USART_RX_BUF[i] - '0');
                    }
                    i++;
                }
                printf("\r\n");
                while (USART_RX_BUF[i] != '\0')
                {
                    if((USART_RX_BUF[i] >= '0') && (USART_RX_BUF[i] <= '9'))
                    {
                        routing_table.node_addr.long_latitude[1] = routing_table.node_addr.long_latitude[1]*10 + USART_RX_BUF[i] - '0';
                        printf("%d", USART_RX_BUF[i] - '0');
                    }
                    i++;
                }
                write_to_flash(((uint64_t)routing_table.node_addr.long_latitude[1] << 32) + routing_table.node_addr.long_latitude[0]);
                
                for(i=0;i<USART_RX_STA;i++)
                    USART_RX_BUF[i]=0;//缓存
                USART_RX_STA=0;
            }
            printf("\r\nlongtitude:%d,latitude:%d\r\n", routing_table.node_addr.long_latitude[0], routing_table.node_addr.long_latitude[1]);
            routing_table.node_addr.addr = read_from_flash();
            xEventGroupClearBits(route_eventgroup_handle, WRITE_ADDR_ORDER);
        }
        
        if((route_eventgroup_bit & WRITE_ADDR_ORDER) && (USART_RX_BUF[0] != ';')) //已经写入过地址了，但需要修改当前地址
        {
            printf((char *)USART_RX_BUF);
            if(USART_RX_STA != 0)
            {
                routing_table.node_addr.long_latitude[0] = 0;
                routing_table.node_addr.long_latitude[1] = 0;
                i = 0;
                while ((USART_RX_BUF[i] != ';') && (USART_RX_BUF[i] != '\0'))
                {
                    if((USART_RX_BUF[i] >= '0') && (USART_RX_BUF[i] <= '9'))
                    {    
                        routing_table.node_addr.long_latitude[0] = routing_table.node_addr.long_latitude[0]*10 + USART_RX_BUF[i] - '0';
                        printf("%d", USART_RX_BUF[i] - '0');
                    }
                    i++;
                }
                printf("\r\n");
                while (USART_RX_BUF[i] != '\0')
                {
                    if((USART_RX_BUF[i] >= '0') && (USART_RX_BUF[i] <= '9'))
                    {
                        routing_table.node_addr.long_latitude[1] = routing_table.node_addr.long_latitude[1]*10 + USART_RX_BUF[i] - '0';
                        printf("%d", USART_RX_BUF[i] - '0');
                    }
                    i++;
                }
                write_to_flash(((uint64_t)routing_table.node_addr.long_latitude[1] << 32) + routing_table.node_addr.long_latitude[0]);
                xEventGroupClearBits(route_eventgroup_handle, WRITE_ADDR_ORDER);
                for(i=0;i<USART_RX_STA;i++)
                    USART_RX_BUF[i]=0;//缓存
                USART_RX_STA=0;
            }
        }
        else if((route_eventgroup_bit & WRITE_ADDR_ORDER) && (USART_RX_BUF[0] == ';'))  vTaskDelete(write_my_addr_handler); //直接发送;表示确认，之后不可以再修改地址，除非复位        
        printf("\r\nlongtitude:%d,latitude:%d\r\n", routing_table.node_addr.long_latitude[0], routing_table.node_addr.long_latitude[1]);
        printf("addr:%llx\r\n", routing_table.node_addr.addr);
        ADDR_MINE = routing_table.node_addr.addr; //写入MAC层的地址
        is_sender_route_handle = xSemaphoreCreateBinary();
        if(is_sender_route_handle != NULL)
        {
            printf("二值信号量创建成功\r\n");
        }
        xSemaphoreGive(is_sender_route_handle); //释放信号量
        vTaskDelay(100);
    }
}

/**
  * @brief  邻居节点与子节点检查函数
  * @param  None
  * @retval None
  */
void node_check_route(void)
{
    delete_dead_neighbor_route();
    DeleteDeadNode(routing_table.tree_pointer, routing_table.tree_pointer);
}

/**
  * @brief  数据上报函数
  * @param  None
  * @retval None
  */
void data_report_route(void)
{
    if(IS_GATWAY)
    {
        printf("开启定时\r\n");
        xTimerStart(send_timer_handle, portMAX_DELAY);
        xTimerChangePeriod(send_timer_handle, 3600000, 0); //调整定时器时间
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, TIMER_OK_4, pdTRUE, pdTRUE, portMAX_DELAY);	//超时时间到且发送空闲
        xSemaphoreTake(is_sender_route_handle, portMAX_DELAY); //获取信号量并死等 
        printf("开启上报数据\r\n");
        send_frame_route.payload.routing_sensor_data.addr_src = routing_table.node_addr.addr;
        SensorDataGet();
        MqttReport(send_frame_route);
        xSemaphoreGive(is_sender_route_handle); //释放信号量，表示重新回到发送空闲
        printf("上报数据完毕\r\n");
    }
    else
    {
        printf("开启定时\r\n");
        xTimerStart(send_timer_handle, portMAX_DELAY);
        xTimerChangePeriod(send_timer_handle, 3600000, 0); //调整定时器时间
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, TIMER_OK_4, pdTRUE, pdTRUE, portMAX_DELAY);	//超时时间到且发送空闲
        xSemaphoreTake(is_sender_route_handle, portMAX_DELAY); //获取信号量并死等 
        printf("开启上报数据\r\n");
        mac_frame_clear(&send_frame);
        send_frame.src_addr = routing_table.node_addr.addr;
        send_frame.dst_addr = routing_table.parent_addr;
        routing_frame_clear(&send_frame_route);
        send_frame_route.route_frame_type = 4;
        send_frame_route.depth = routing_table.tree_depth;
        send_frame_route.payload.routing_sensor_data.addr_src = routing_table.node_addr.addr;
        SensorDataGet();
        memcpy((&send_frame)->payload, &send_frame_route, sizeof(RoutingFrame));	    
        if(mac_send_without_data_recv() == 0) xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN); //如果发送失败则进入未入网状态，重新选择父节点  
        xSemaphoreGive(is_sender_route_handle); //释放信号量，表示重新回到发送空闲
        printf("上报数据完毕\r\n");
    }
}

void SensorDataGet(void)
{
    uint32_t raw_data = 0;
    UBaseType_t uxHighWaterMark;
    sensor_power_on();
    delay_ms(50);
    sht45init();
    raw_data = SHT45_ReadRawData(1);
    send_frame_route.payload.routing_sensor_data.temperature = raw_data & 0xFFFF;
    send_frame_route.payload.routing_sensor_data.humidity = raw_data >> 16;
    printf("Temperature: %f, Humidity:%f\r\n", (-45 + 175*(send_frame_route.payload.routing_sensor_data.temperature)/65535.0), (-6 + 125*(send_frame_route.payload.routing_sensor_data.humidity)/65535.0));
    if(BMP280()){
        delay_ms(50);
        raw_data = bmp280GetRawData();
    }
    else raw_data = 0;
    send_frame_route.payload.routing_sensor_data.pressure = raw_data;
    printf("pressure:%f\r\n", send_frame_route.payload.routing_sensor_data.pressure/256.0f);
		delay_ms(100);
    raw_data = CurrentSoilstate(0x05);
    send_frame_route.payload.routing_sensor_data.soilstate1 = raw_data;
		delay_ms(100);
    raw_data = CurrentSoilstate(0x06);
    send_frame_route.payload.routing_sensor_data.soilstate2 = raw_data;
    delay_ms(100);
    raw_data = CurrentSoilstate(0x07);
    send_frame_route.payload.routing_sensor_data.soilstate3 = raw_data;
		delay_ms(100);
    raw_data =  CurrentPrecipitation();
    send_frame_route.payload.routing_sensor_data.precipitation = raw_data & 0xFFFF;
    CleanPrecipitation();
		delay_ms(100);
    raw_data = CurrentRadiation();
    send_frame_route.payload.routing_sensor_data.radiation = raw_data & 0xFFFF;
		delay_ms(100);
    raw_data = CurrentWindsSpeed();
    send_frame_route.payload.routing_sensor_data.windspeed = raw_data & 0xFFFF;
		delay_ms(100);
    raw_data = CurrentWindsDirection();
    send_frame_route.payload.routing_sensor_data.wind_direction = raw_data & 0xFFFF; 
    sensor_power_off();
    uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL); 
    printf("send_timer任务使用情况：%ld\r\n",uxHighWaterMark);
    
}
/**
  * @brief  数据转发函数
  * @param  None
  * @retval None
  */
void data_relay_route(void)
{
    if(IS_GATWAY)
    {
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle,  RELAY_TASK_START, pdTRUE, pdTRUE, portMAX_DELAY);	//超时时间到且发送空闲
        xSemaphoreTake(is_sender_route_handle, portMAX_DELAY); //获取信号量并死等 
        printf("开启转发数据\r\n");
        MqttReport(recv_frame_route);
        printf("转发数据的源节点地址：%llx\r\n", relay_frame_route.payload.routing_sensor_data.addr_src);
        xSemaphoreGive(is_sender_route_handle); //释放信号量，表示重新回到发送空闲
        printf("转发数据完毕\r\n");
    }
    else
    {
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, RELAY_TASK_START, pdTRUE, pdTRUE, portMAX_DELAY);	//超时时间到且发送空闲
        xSemaphoreTake(is_sender_route_handle, portMAX_DELAY); //获取信号量并死等 
        printf("开启转发数据\r\n");
        mac_frame_clear(&send_frame);
        routing_frame_clear(&send_frame_route);
        memcpy(&send_frame_route, &relay_frame_route, sizeof(RoutingFrame));
        send_frame_route.route_frame_type = 4;
        send_frame_route.depth = routing_table.tree_depth;    
        send_frame.src_addr = routing_table.node_addr.addr;
        send_frame.dst_addr = routing_table.parent_addr;
        memcpy((&send_frame)->payload, &send_frame_route, sizeof(RoutingFrame));	    
        if(mac_send_without_data_recv() == 0) xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN); //如果发送失败则进入未入网状态，重新选择父节点
        xSemaphoreGive(is_sender_route_handle); //释放信号量，表示重新回到发送空闲
        printf("转发数据完毕\r\n");      
    }
}

/**
  * @brief  4g模块接入阿里云函数
  * @param  None
  * @retval None
  */
void MqttConnect(void)
{
    u8 res = 1; 
    u16 errcont = 0;
    while(res)
    {
  //         IWDG_Feed();//喂狗
        res=EC20_CONNECT_SERVER_CFG_INFOR((u8 *)PRODUCTKEY,(u8 *)DEVICENAME,(u8 *)DEVICESECRET);   //接入阿里云
        delay_ms(1000);
        printf("连接次数: %d次\r\n",errcont);
        errcont++;
        if(errcont > 50)
        {
            reset_4g();
            __set_FAULTMASK(1);
            NVIC_SystemReset();	//超时重启
            break;
        }
    }
}


/**
  * @brief  4g模块上报数据函数
  * @param  report_data_frame：要上报的数据帧
  * @retval None
  */
void MqttReport(RoutingFrame report_data_frame)
{    
    memset(send_data_4g,0,BUFLEN);//AtStrBuf_EC800清零
    sprintf(send_data_4g, "{params:{F:\"%llx %x %x %x %x %x %x %x %x %x %x\"}}",
            report_data_frame.payload.routing_sensor_data.addr_src, report_data_frame.payload.routing_sensor_data.temperature, 
            report_data_frame.payload.routing_sensor_data.humidity, report_data_frame.payload.routing_sensor_data.pressure,
            report_data_frame.payload.routing_sensor_data.soilstate1, report_data_frame.payload.routing_sensor_data.soilstate2,
            report_data_frame.payload.routing_sensor_data.soilstate3, report_data_frame.payload.routing_sensor_data.precipitation,
            report_data_frame.payload.routing_sensor_data.windspeed, report_data_frame.payload.routing_sensor_data.wind_direction,
            report_data_frame.payload.routing_sensor_data.radiation);
    printf(send_data_4g);
    EC20_MQTT_SEND_DATA((u8 *)PRODUCTKEY,(u8 *)DEVICENAME,(u8 *)send_data_4g);
}

/**
  * @brief  路由更新发送函数
  * @param  None
  * @retval None
  */
void update_broadcast_route(void)
{
    route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, UPDATE_TASK_START, pdTRUE, pdTRUE, portMAX_DELAY);	
    xSemaphoreTake(is_sender_route_handle, portMAX_DELAY); //获取信号量并死等 
    printf("开启发送路由更新\r\n");
    mac_frame_clear(&send_frame);
    routing_frame_clear(&send_frame_route);
    memcpy(&send_frame_route, &update_frame_route, sizeof(RoutingFrame));
    send_frame_route.route_frame_type = 3;
    send_frame_route.depth = routing_table.tree_depth;    
    send_frame.src_addr = routing_table.node_addr.addr;
    send_frame.dst_addr = routing_table.parent_addr;
    memcpy((&send_frame)->payload, &send_frame_route, sizeof(RoutingFrame));	    
    mac_send_broadcast();
    xSemaphoreGive(is_sender_route_handle); //释放信号量，表示重新回到发送空闲
    printf("转发数据完毕\r\n");      
    
}
/**
  * @brief  入网请求发送函数
  * @param  newfather_addr：新的父节点地址
  * @retval None
  */
void net_access_req(NodeAddr newfather_addr)
{
    xSemaphoreTake(is_sender_route_handle, portMAX_DELAY); //获取信号量并死等 
    printf("发送入网请求\r\n");
    mac_frame_clear(&send_frame);
    send_frame.src_addr = routing_table.node_addr.addr;
    send_frame.dst_addr = newfather_addr;
    routing_frame_clear(&send_frame_route);
    send_frame_route.route_frame_type = 1;
    send_frame_route.depth = routing_table.tree_depth;
    memcpy((&send_frame)->payload, &send_frame_route, sizeof(RoutingFrame));	
    if(mac_send_require_data_recv() == 0) xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN); //如果发送失败则进入未入网状态，重新选择父节点
    xSemaphoreGive(is_sender_route_handle); //释放信号量，表示重新回到发送空闲
    printf("发送入网请求完毕\r\n");
}


/**
  * @brief  入网函数
  * @param  None
  * @retval None
  */
void join_wan_route(u8 *number_neighbor, u8 *rssi_neighbor, TreeNode* result)
{
    if(routing_table.tree_pointer == NULL) routing_table.tree_pointer = createNode(routing_table.node_addr.addr);  
    if(IS_GATWAY)
    {
        routing_table.tree_depth = 0; 
        routing_table.join_flag = 0;
        routing_table.parent_addr = 0xffffffffffffffff;
        routing_table.parent_rssi = 0;
        printf("网关入网成功！\r\n");
        xEventGroupClearBits(route_eventgroup_handle, IS_JOIN_WAN);
        xEventGroupSetBits(route_eventgroup_handle, BEACON_TASK_START); //启动Beacon发送任务
    }
    else  //不是网关的节点
    {
        routing_table.parent_addr = NULL;
        routing_table.tree_depth = 0xff;
        if(routing_table.neighbor_count > 0)
        {
            search_min_rssi_neighbor_route(number_neighbor, rssi_neighbor);
            FindTarget(routing_table.tree_pointer, routing_table.neighbors[*number_neighbor].addr, &result);
//            printf("向%llx发送入网请求\r\n", routing_table.neighbors[number_neighbor].addr);
//            printf("当前节点地址%llx\r\n", routing_table.node_addr.addr);
            if(result == NULL)  //该邻居节点不是自己的子节点
            {
                net_access_req(routing_table.neighbors[*number_neighbor].addr); //向新节点发送入网请求，成功才能清除入网标志位 
            }
            delete_neighbor_route(routing_table.neighbors[*number_neighbor].addr);
            if(routing_table.parent_addr != NULL) //父节点地址不为空
            {
//                printf("入网成功,父节点：%llx, 当前深度%d！\r\n", routing_table.parent_addr, routing_table.tree_depth);
                printf("入网成功\r\n");
                xEventGroupClearBits(route_eventgroup_handle, IS_JOIN_WAN);
            }
        }
    }
}


/**
  * @brief  清空路由帧
	*													 
  * @param  routing_frame:要清除的帧
  * @retval 
  */
void routing_frame_clear(RoutingFrame* routing_frame)
{
		memset(routing_frame, 0, sizeof(RoutingFrame));//清空结构体成员数据
}

/**
  * @brief  Beacon发送函数，定时发送，且只有网关才发送Beacon
  * @param  None
  * @retval None
  */
void beacon_send_route(void)
{
    xSemaphoreTake(is_sender_route_handle, portMAX_DELAY); //获取信号量并死等 
    printf("广播发送Beacon\r\n");
    mac_frame_clear(&send_frame);
    send_frame.src_addr = routing_table.node_addr.addr;
    send_frame.dst_addr = routing_table.parent_addr;
    routing_frame_clear(&send_frame_route);
    send_frame_route.route_frame_type = 6;
    send_frame_route.depth = routing_table.tree_depth;
    memcpy((&send_frame)->payload, &send_frame_route, sizeof(RoutingFrame));	
    mac_send_broadcast();
    xSemaphoreGive(is_sender_route_handle); //释放信号量，表示重新回到发送空闲
    printf("发送Beacon完毕\r\n");
    printf("开启Beacon发送定时器定时\r\n");
    xTimerStart(beacon_send_timer_handle, portMAX_DELAY);
    route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, BEACON_TIMER_OK, pdTRUE, pdTRUE, portMAX_DELAY);	//超时时间到且发送空闲
    
}

/**
  * @brief  路由层数据包接收函数，将接收到的MAC帧的负载转变为路由帧
  * @param  None
  * @retval None
  */
void packet_receive_route(void)
{
    routing_frame_clear(&recv_frame_route);//先清空上一次的再赋值
    memcpy(&recv_frame_route, (&recv_frame)->payload, sizeof(RoutingFrame));	
}

/**
  * @brief  路由层数据包处理函数
  * @param  None
  * @retval None
  */
void packet_process_route(void)
{
    TreeNode* child1 = NULL;
    TreeNode* child2 = NULL;
    u8 number = 0;
    while(1)
    {
        printf("等待mac接收数据型帧\r\n");
        recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, ROUTE_PACKET_RECV_SENDER | ROUTE_PACKET_RECV_RECEIVCER | ROUTE_PACKET_RECV_SLEEP, pdTRUE, pdFALSE, portMAX_DELAY);
        printf("路由层接收到数据型帧了！！！！\r\n");
				packet_receive_route();
        if(recv_eventgroup_bit & ROUTE_PACKET_RECV_SENDER)
        {
            if(recv_frame_route.route_frame_type == 2)
            {
                printf("接收到正确的入网回复了,发送数据型ACK\r\n");
                routing_table.tree_depth = recv_frame_route.depth + 1;
                routing_table.parent_addr = recv_frame.src_addr;
                routing_table.parent_rssi = recv_rssi; 
                // 赋值数据型ACK给发送缓冲
                mac_frame_clear(&send_frame);
                routing_frame_clear(&send_in_recv_frame_route);
                send_frame.src_addr = routing_table.node_addr.addr;
                send_frame.dst_addr = recv_frame.src_addr;
                send_in_recv_frame_route.route_frame_type = 5;
                send_in_recv_frame_route.depth = routing_table.tree_depth;
                memcpy((&send_frame)->payload, &send_in_recv_frame_route, sizeof(RoutingFrame));	

            }
            if(recv_frame_route.route_frame_type == 0)
            {
                printf("当前发送信息被拒绝,发送数据型ACK\r\n");
                // 赋值数据型ACK给发送缓冲
                mac_frame_clear(&send_frame);
                routing_frame_clear(&send_in_recv_frame_route);
                send_frame.src_addr = routing_table.node_addr.addr;
                send_frame.dst_addr = recv_frame.src_addr;
                send_in_recv_frame_route.route_frame_type = 5;
                send_in_recv_frame_route.depth = routing_table.tree_depth;
                memcpy((&send_frame)->payload, &send_in_recv_frame_route, sizeof(RoutingFrame));	
                xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN);
            }
            
        }
        else if(recv_eventgroup_bit & ROUTE_PACKET_RECV_RECEIVCER)
        {
            if(recv_frame_route.route_frame_type == 1)
            { 
                //如果自身入网了且不是自己的父节点，则处理该入网请求
                route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdFALSE, 0);
                if((!(route_eventgroup_bit & IS_JOIN_WAN)) && (recv_frame.src_addr != routing_table.parent_addr)) 
                {
                    number = search_neighbor_route(recv_frame.src_addr);
                    if(number != 0xff)  //是自己的邻居节点，则删除后再处理
                    {
                        delete_neighbor_route(routing_table.neighbors[number].addr);
                    }
                    FindTarget(routing_table.tree_pointer, recv_frame.src_addr, &child1); //判断源节点是否已经是自己的子节点
                    if(child1 != NULL)  //已经是自己的子节点
                    {
                        printf("更新子节点%llx\r\n", recv_frame.src_addr);
                        ChangeChild(routing_table.tree_pointer, routing_table.tree_pointer, child1);
                        child1->is_node_alive = 1;
                    }
                    else
                    {
                        printf("添加子节点%llx, 发送路由更新\r\n", recv_frame.src_addr);
                        AddChild(routing_table.tree_pointer, createNode(recv_frame.src_addr));
                        routing_frame_clear(&update_frame_route);
                        update_frame_route.payload.routing_control.control_code = 1;
                        update_frame_route.payload.routing_control.father = routing_table.node_addr.addr;
                        update_frame_route.payload.routing_control.child = recv_frame.src_addr;
                        xEventGroupSetBits(route_eventgroup_handle, UPDATE_TASK_START);  
                        routing_table.child_count ++;
                    }
                    mac_frame_clear(&send_frame);
                    routing_frame_clear(&send_in_recv_frame_route);
                    send_frame.dst_addr = recv_frame.src_addr;
                    send_frame.src_addr = routing_table.node_addr.addr; 
                    send_in_recv_frame_route.route_frame_type = 2;
                    send_in_recv_frame_route.depth = routing_table.tree_depth;
                    memcpy((&send_frame)->payload, &send_in_recv_frame_route, sizeof(RoutingFrame));
                    lora_Send_DATA_MACFrame(&send_frame,WITHOUT_DATA_RECV);
                }
                else  //如果是自己的父节点，或自身未入网，则拒绝请求
                {
                    printf("重新选择父节点\r\n");
                    mac_frame_clear(&send_frame);
                    routing_frame_clear(&send_in_recv_frame_route);
                    send_frame.dst_addr = recv_frame.src_addr;
                    send_frame.src_addr = routing_table.node_addr.addr; 
                    send_in_recv_frame_route.route_frame_type = 0;
                    send_in_recv_frame_route.depth = routing_table.tree_depth;
                    memcpy((&send_frame)->payload, &send_in_recv_frame_route, sizeof(RoutingFrame));
                    lora_Send_DATA_MACFrame(&send_frame,WITHOUT_DATA_RECV);
                    xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN);
                }
            }
            if(recv_frame_route.route_frame_type == 5)  //数据型ACK回复
            {
                
            }
            if(recv_frame_route.route_frame_type == 4)  //上报传感器数据
            {
                //如果自身入网了且当前发送方不是自己的父节点且数据包源地址不是自己的父节点，则处理该入网请求
                route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdFALSE, 0);
                if(recv_frame_route.payload.routing_sensor_data.addr_src != 0)
                {
                    if((!(route_eventgroup_bit & IS_JOIN_WAN)) && (recv_frame.src_addr != routing_table.parent_addr) && (recv_frame_route.payload.routing_sensor_data.addr_src != routing_table.parent_addr))  
                    {
                        number = search_neighbor_route(recv_frame.src_addr);
                        if(number != 0xff)  //当前数据帧发送方是自己的邻居节点，则删除后再处理
                        {
                            delete_neighbor_route(routing_table.neighbors[number].addr);
                        }
                        number = search_neighbor_route(recv_frame_route.payload.routing_sensor_data.addr_src);
                        if(number != 0xff)  //数据包源地址是自己的邻居节点，则删除后再处理
                        {
                            delete_neighbor_route(routing_table.neighbors[number].addr);
                        }
                        FindTarget(routing_table.tree_pointer, recv_frame.src_addr, &child1); 
                        if(child1 != NULL)  //已经是自己的子节点
                        {
                            printf("更新子节点%llx\r\n", recv_frame.src_addr);
                            ChangeChild(routing_table.tree_pointer, routing_table.tree_pointer, child1);
                            child1->is_node_alive = 1;
                        }
                        else
                        {
                            printf("添加子节点%llx, 发送路由更新\r\n", recv_frame.src_addr);
                            AddChild(routing_table.tree_pointer, createNode(recv_frame.src_addr));
                            routing_frame_clear(&update_frame_route);
                            update_frame_route.payload.routing_control.control_code = 1;
                            update_frame_route.payload.routing_control.father = routing_table.node_addr.addr;
                            update_frame_route.payload.routing_control.child = recv_frame.src_addr;
                            xEventGroupSetBits(route_eventgroup_handle, UPDATE_TASK_START);                       
                            routing_table.child_count ++;
                        }
                        if(recv_frame.src_addr != recv_frame_route.payload.routing_sensor_data.addr_src)
                        {
                            FindTarget(routing_table.tree_pointer, recv_frame_route.payload.routing_sensor_data.addr_src, &child2); //传感器数据源节点的地址 
                            if(child2 != NULL)  //已经是自己的子节点
                            {
                                printf("更新子节点%llx\r\n", recv_frame_route.payload.routing_sensor_data.addr_src);
                                ChangeChild(routing_table.tree_pointer, child1, child2);
                                child1->is_node_alive = 1;
                                child2->is_node_alive = 1;
                            }
                            else  //如果传感器数据源节点地址不是自己的子节点，那么该节点大概率是自己子节点的子节点，但由于并不知道到底是哪个子节点的儿子，所以姑且认为是当前节点的子节点吧
                            {
                                printf("添加子节点%llx, 发送路由更新\r\n", recv_frame_route.payload.routing_sensor_data.addr_src);
                                AddChild(routing_table.tree_pointer, createNode(recv_frame_route.payload.routing_sensor_data.addr_src));
                                routing_frame_clear(&update_frame_route);
                                update_frame_route.payload.routing_control.control_code = 1;
                                update_frame_route.payload.routing_control.father = routing_table.node_addr.addr;
                                update_frame_route.payload.routing_control.child = recv_frame.src_addr;
                                xEventGroupSetBits(route_eventgroup_handle, UPDATE_TASK_START);   
                                routing_table.child_count ++;
                            }
                        }
                        printf("启动数据转发任务\r\n");
                        memcpy(&relay_frame_route, &recv_frame_route, sizeof(RoutingFrame));
                        xEventGroupSetBits(route_eventgroup_handle, RELAY_TASK_START);  //启动消息转发
                    }
                    else
                    {
                        printf("重新选择父节点\r\n");
                        mac_frame_clear(&send_frame);
                        routing_frame_clear(&send_in_recv_frame_route);
                        send_frame.dst_addr = recv_frame.src_addr;
                        send_frame.src_addr = routing_table.node_addr.addr; 
                        send_in_recv_frame_route.route_frame_type = 0;
                        send_in_recv_frame_route.depth = routing_table.tree_depth;
                        memcpy((&send_frame)->payload, &send_in_recv_frame_route, sizeof(RoutingFrame));
                        lora_Send_DATA_MACFrame(&send_frame,WITHOUT_DATA_RECV);
                        xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN);
                    }
                }
            }
            
        }
        else if(recv_eventgroup_bit & ROUTE_PACKET_RECV_SLEEP)  //该状态下接收到的路由帧不可以立刻回复
        {
            if((recv_frame_route.route_frame_type == 4) || (recv_frame_route.route_frame_type == 6))  //上报传感器数据或是邀请节点入网Beacon
            {    
                if(recv_frame.dst_addr != routing_table.node_addr.addr) //是别的节点的数据包，但目的地不是自己
                {
                    if(recv_frame.src_addr == routing_table.parent_addr)  //源节点是自己的父节点，则检查目的节点是否是自己的子节点
                    {
                        FindTarget(routing_table.tree_pointer, recv_frame.dst_addr, &child2); //判断目的节点是否是自己的子节点
                        if((child2 != NULL) || (recv_frame.dst_addr == 0)) //如果目的节点是自己的子节点，或父节点未入网，则马上切换父节点，防止造成回路
                        {
                            printf("重新选择父节点\r\n");
                            xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN);
                        }
                        else  //如果目的节点不是自己的子节点，更新父节点rssi和自身深度 
                        {
                            printf("接收到父节点的BEACON，更新父节点的信息\r\n");
                            routing_table.parent_rssi = recv_rssi;
                            routing_table.tree_depth = recv_frame_route.depth + 1;
                        }
                    }
                    else  //源节点不是是自己的父节点
                    {
                        FindTarget(routing_table.tree_pointer, recv_frame.src_addr, &child1); //判断源节点是否是自己的子节点
                        FindTarget(routing_table.tree_pointer, recv_frame.dst_addr, &child2); //判断目的节点是否是自己的子节点
                        if(recv_frame_route.payload.routing_sensor_data.addr_src == routing_table.parent_addr)  //是数据上报型路由帧,且源地址是自己的父节点
                        {
                            if((child1 != NULL) || (child2 != NULL)) //如果源地址和目的地址有一个是自己的子节点，且数据包的源地址是自己的父节点，说明出现了环，进入未入网
                            {
                                printf("重新选择父节点\r\n");
                                xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN);
                            }
                        }
                        if ((child1 != NULL) && (child2 == NULL) && (child1->addr != routing_table.node_addr.addr)) //如果是自己的子节点，且目的地不是自己的子节点，则要删除该节点及其子树
                        {
                            route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdTRUE, 0); 
                            printf("删除子节点%llx,如果入网发送路由更新", child1->addr);
                            if(!(route_eventgroup_bit & IS_JOIN_WAN)) //如果已经入网
                            {
                                routing_frame_clear(&update_frame_route);
                                update_frame_route.payload.routing_control.control_code = 0;
                                update_frame_route.payload.routing_control.father = routing_table.node_addr.addr;
                                update_frame_route.payload.routing_control.child = child1->addr;
                                xEventGroupSetBits(route_eventgroup_handle, UPDATE_TASK_START);
                            }
                            DelSubtree(routing_table.tree_pointer, child1); 
                            routing_table.child_count --;
                        }
                        else if((child1 == NULL) && (child2 != NULL)) //如果源节点不是自己的子节点，但目的节点是自己的子节点,则将该源节点加入到自己的子节点列表中。
                        {
                            printf("添加子节点%llx\r\n", recv_frame.src_addr);
                            child1 = createNode(recv_frame.src_addr);
                            AddChild(child2, child1);
                            routing_table.child_count ++;
                            number = search_neighbor_route(recv_frame.src_addr);
                            if(number != 0xff)  //是自己的邻居节点，则删除
                            {
                                delete_neighbor_route(routing_table.neighbors[number].addr);
                            }
                            
                        }
                        else if((child1 == NULL) && (child2 == NULL) && (recv_frame.dst_addr != 0)) //如果源节点和目的节点都不是自己的子节点，并且消息的目的地不是0（说明已经入网），则判断是否可以将该源节点加入到自己的邻居列表中
                        {
                            printf("添加邻居%llx\r\n", recv_frame.src_addr);
                            add_neighbor_route(recv_frame.src_addr, recv_rssi, recv_frame_route.depth);
                        }
                        else if((child1 != NULL) && (child2 != NULL)) //如果源节点和目的节点都是自己的子节点，则更新拓扑结构
                        {
                            printf("更新子节点 %llx，%llx \r\n", child2->addr, child1->addr);
//                            ChangeChild(routing_table.tree_pointer, child2, child1);
                            child1->is_node_alive = 1;
                            child2->is_node_alive = 1;
                        }
                    }
                }
            }
            else if(recv_frame_route.route_frame_type == 3)  //路由更新,子节点的删除、添加，路由帧负载只需包含子节点及其新父节点地址
            {
                route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdTRUE, 0);
                if(!(route_eventgroup_bit & IS_JOIN_WAN)) //如果已经入网
                {
                    route_update_process();
                }
                else  //如果没有入网
                {
                    FindTarget(routing_table.tree_pointer, recv_frame.src_addr, &child1); //判断源节点是否是自己的子节点
                    FindTarget(routing_table.tree_pointer, recv_frame.dst_addr, &child2); //判断目的节点是否是自己的子节点
                    if((child1 == NULL) && (child2 == NULL) && (recv_frame.dst_addr != 0)) //如果源节点和目的节点都不是自己的子节点，并且消息的目的地不是0（说明已经入网），则判断是否可以将该源节点加入到自己的邻居列表中
                    {
                        printf("添加邻居%llx\r\n", recv_frame.src_addr);
                        add_neighbor_route(recv_frame.src_addr, recv_rssi, recv_frame_route.depth);
                    }
                    else if ((child1 != NULL) && (child2 == NULL) && (child1->addr != routing_table.node_addr.addr)) //如果是自己的子节点，且目的地不是自己的子节点，则要删除该节点及其子树
                    {
                        printf("删除子节点%llx", child1->addr);
                        DelSubtree(routing_table.tree_pointer, child1); 
                    }
                }
            }
            else if(recv_frame_route.route_frame_type == 1) //入网请求
            {
                FindTarget(routing_table.tree_pointer, recv_frame.src_addr, &child1); //判断源节点是否是自己的子节点
                FindTarget(routing_table.tree_pointer, recv_frame.dst_addr, &child2); //判断目的节点是否是自己的子节点
                if((child1 != NULL) && (child2 == NULL) && (child1->addr != routing_table.node_addr.addr)) //源节点是自己的子节点而目的节点不是自己或自己的子节点，则直接删除
                {
                    printf("删除子节点%llx", child1->addr);
                    if(!(route_eventgroup_bit & IS_JOIN_WAN)) //如果已经入网，发送路由更新
                    {
                        routing_frame_clear(&update_frame_route);
                        update_frame_route.payload.routing_control.control_code = 0;
                        update_frame_route.payload.routing_control.father = routing_table.node_addr.addr;
                        update_frame_route.payload.routing_control.child = child1->addr;
                        xEventGroupSetBits(route_eventgroup_handle, UPDATE_TASK_START);
                    }
                    DelSubtree(routing_table.tree_pointer, child1); 
                }
            }
            else if(recv_frame_route.route_frame_type == 2) //入网回复
            {
                FindTarget(routing_table.tree_pointer, recv_frame.src_addr, &child1); //判断源节点是否是自己的子节点
                FindTarget(routing_table.tree_pointer, recv_frame.dst_addr, &child2); //判断目的节点是否是自己的子节点
                if((child1 == NULL) && (child2 != NULL) && (child2->addr != routing_table.node_addr.addr)) //源节点不是自己的子节点而目的节点是自己的子节点，则直接删除
                {
                    printf("删除子节点%llx", child2->addr);
                    if(!(route_eventgroup_bit & IS_JOIN_WAN)) //如果已经入网，发送路由更新
                    {
                        routing_frame_clear(&update_frame_route);
                        update_frame_route.payload.routing_control.control_code = 0;
                        update_frame_route.payload.routing_control.father = routing_table.node_addr.addr;
                        update_frame_route.payload.routing_control.child = child2->addr;
                        xEventGroupSetBits(route_eventgroup_handle, UPDATE_TASK_START);
                    }
                    DelSubtree(routing_table.tree_pointer, child2); 
                    printf("添加邻居%llx\r\n", recv_frame.src_addr);
                    add_neighbor_route(recv_frame.src_addr, recv_rssi, recv_frame_route.depth);
                }
            }
				}   
        vTaskDelay(10);
    }
}

/**
  * @brief  添加邻居节点，注意，每次添加邻居节点之前都要检查该节点是否是自己的子节点，同理，每次添加子节点之前都要检查该节点是否是自己的父节点或邻居节点
  * @param  addr：待添加邻居的地址；rssi：待添加邻居的信号强度；depth:待添加邻居的深度
  * @retval None
  */
void add_neighbor_route(NodeAddr addr, uint8_t rssi, uint8_t depth)
{
    u8 number = 0, max_number = 0, max_rssi = 0;
    number = search_neighbor_route(addr);   
    if(number == 0xff)  //新节点
    {
        if(routing_table.neighbor_count < MAX_NEIGHBORS)  //邻居节点个数小于MAX_NEIGHBORS，直接添加
        {
            routing_table.neighbors[routing_table.neighbor_count].addr = addr;
            routing_table.neighbors[routing_table.neighbor_count].depth = depth;
            routing_table.neighbors[routing_table.neighbor_count].rssi = rssi;
            routing_table.neighbors[routing_table.neighbor_count].is_neighbors_alive = 1;
            routing_table.neighbor_count ++;
        }
        else  //邻居节点个数等于于MAX_NEIGHBORS，判断是否可以替换
        {
            search_max_rssi_neighbor_route(&max_number, &max_rssi);
            if(rssi <= max_rssi)  //新节点的信号强度更大，则替换
            {
                routing_table.neighbors[max_number].addr = addr;
                routing_table.neighbors[max_number].depth = depth;
                routing_table.neighbors[max_number].rssi = rssi;
                routing_table.neighbors[max_number].is_neighbors_alive = 1;
            }
        }
    }
    else  //已经在邻居节点列表中，更新rssi和深度
    {
        routing_table.neighbors[number].depth = depth;
        routing_table.neighbors[number].rssi = rssi;
        routing_table.neighbors[number].is_neighbors_alive = 1;
    }
}

/**
  * @brief  寻找邻居节点
  * @param  addr：待寻找邻居的地址
  * @retval 0xff:未找到该节点，其他：返回该节点所在下标
  */        
u8 search_neighbor_route(NodeAddr addr)
{
    u8 i = 0;
    for(i = 0; i < routing_table.neighbor_count; i ++)
    {
        if(routing_table.neighbors[i].addr == addr) return i;       
    } 
    return 0xff;
}

/**
  * @brief  寻找rssi最大的邻居节点，rssi值越大，代表信号强度越小
  * @param  number:用于存储最大rssi节点的坐标；rssi：用于存储最大的rssi值
  * @retval None
  */        
void search_max_rssi_neighbor_route(u8* number, u8* rssi)
{
    u8 i = 0;
    *rssi = 0;
    for(i = 0; i < routing_table.neighbor_count; i ++)
    {
        if(routing_table.neighbors[i].rssi >= *rssi)
        {
            *rssi = routing_table.neighbors[i].rssi;
            *number = i;
        }          
    } 
}

/**
  * @brief  寻找rssi最小的邻居节点，rssi值越小，代表信号强度越大
  * @param  number:用于存储最大rssi节点的坐标；rssi：用于存储最大的rssi值
  * @retval None
  */        
void search_min_rssi_neighbor_route(u8* number, u8* rssi)
{
    u8 i = 0;
    *rssi = 0xff;
    for(i = 0; i < routing_table.neighbor_count; i ++)
    {
        if(routing_table.neighbors[i].rssi <= *rssi)
        {
            *rssi = routing_table.neighbors[i].rssi;
            *number = i;
        }          
    } 
}

/**
  * @brief  删除指定的邻居节点
  * @param  addr_neighbor:要删除的邻居节点的地址
  * @retval None
  */        
void delete_neighbor_route(NodeAddr addr_neighbor)
{
    u8 src = 0, dst = 0;
    while(src < routing_table.neighbor_count)
    {
        if(routing_table.neighbors[src].addr == addr_neighbor)  src ++;
        else  
        {
            routing_table.neighbors[dst].addr = routing_table.neighbors[src].addr;
            routing_table.neighbors[dst].depth = routing_table.neighbors[src].depth;
            routing_table.neighbors[dst].rssi = routing_table.neighbors[src].rssi;
            routing_table.neighbors[dst].is_neighbors_alive = routing_table.neighbors[src].is_neighbors_alive;
            dst ++;
            src ++;
        } 
    }
    if(src != dst)  //删除了邻居节点
        routing_table.neighbor_count --;
}

/**
  * @brief  删除死掉的邻居节点
  * @param  None
  * @retval None
  */ 
void delete_dead_neighbor_route(void)
{
    uint8_t i = 0, count = 0, j = 0;
    NodeAddr addr = 0;
    count = routing_table.neighbor_count;
    for(i = 0; i < count; i ++)
    {
        if(routing_table.neighbors[j].is_neighbors_alive == 0)
        {
            addr = routing_table.neighbors[j].addr;
            printf("要删除的neighbor:%llx\r\n", addr);
            delete_neighbor_route(addr);
        }
        else
        {
            j ++;
        }
    }        
}

/**
  * @brief  处理路由更新函数
  * @param  None
  * @retval None
  */ 
void route_update_process(void)
{
    TreeNode *father, *child, *result;
    FindTarget(routing_table.tree_pointer, recv_frame_route.payload.routing_control.father, &father);
    FindTarget(routing_table.tree_pointer, recv_frame_route.payload.routing_control.child, &child);
    FindFather(routing_table.tree_pointer, child, &result);
    if(recv_frame_route.payload.routing_control.control_code == 0) //有节点的子节点被删除
    {
        if((recv_frame_route.payload.routing_control.father == routing_table.parent_addr) && (recv_frame_route.payload.routing_control.child == routing_table.node_addr.addr)) //如果父节点删除的是自己
        {
            printf("被父节点删除，重新选择父节点\r\n");
            xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN);
            return;
        }
        if((father != NULL) && (child != NULL) && (result != NULL) &&(father->addr == result->addr)) //两个节点都是自己的子节点，并且father指向的节点是child指向的节点的父亲，则自己也删除被删除的子节点
        {
            father->is_node_alive = 1;
            printf("删除子节点%llx,并转发路由更新\r\n", child->addr);
            routing_frame_clear(&update_frame_route);
            update_frame_route.payload.routing_control.control_code = 0;
            update_frame_route.payload.routing_control.father = recv_frame_route.payload.routing_control.father;
            update_frame_route.payload.routing_control.child = recv_frame_route.payload.routing_control.child;
            DelSubtree(routing_table.tree_pointer, child);  
            routing_table.child_count --;
            xEventGroupSetBits(route_eventgroup_handle, UPDATE_TASK_START);     
            return;
        }
    }
    else if(recv_frame_route.payload.routing_control.control_code == 1) //有节点的子节点被添加
    {
        if((child != NULL) && (father == NULL) && (child->addr != routing_table.node_addr.addr)) //其他节点添加了自己的子节点，则删除自己的子节点
        {
            printf("子节点%llx添加到新节点%llx去了,删除并转发路由更新\r\n", child->addr, recv_frame_route.payload.routing_control.father);
            routing_frame_clear(&update_frame_route);
            update_frame_route.payload.routing_control.control_code = 1;
            update_frame_route.payload.routing_control.father = recv_frame_route.payload.routing_control.father;
            update_frame_route.payload.routing_control.child = recv_frame_route.payload.routing_control.child;
            DelSubtree(routing_table.tree_pointer, child);
            routing_table.child_count --;
            xEventGroupSetBits(route_eventgroup_handle, UPDATE_TASK_START); 
            return;
        }
        if((recv_frame_route.payload.routing_control.father == routing_table.parent_addr) && (father != NULL)) //父节点是自己的子节点，则未入网
        {
            printf("重新选择父节点\r\n");
            xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN);
            return;
        }
        if(father != NULL)  //如果是自己子节点添加了子节点，则判断该添加的子节点是否已经是自己的子节点，若是则修改，若不是则添加
        {
            if((child != NULL) && (result->addr == father->addr))
            {
                return;
            }
            if((child != NULL) && (result->addr != father->addr))
            {
                printf("更新拓扑结构，子节点 %llx 为子节点 %llx 的父节点,并转发路由更新\r\n", father->addr, child->addr);
                ChangeChild(routing_table.tree_pointer, father, child);
                father->is_node_alive = 1;
                child->is_node_alive = 1;
            }
            else if(child == NULL)
            {
                printf("添加子节点%llx,并转发路由更新\r\n", recv_frame_route.payload.routing_control.child);
                AddChild(father, createNode(recv_frame_route.payload.routing_control.child));
                routing_table.child_count ++;    
            }
            routing_frame_clear(&update_frame_route);
            update_frame_route.payload.routing_control.control_code = 1;
            update_frame_route.payload.routing_control.father = recv_frame_route.payload.routing_control.father;
            update_frame_route.payload.routing_control.child = recv_frame_route.payload.routing_control.child;
            xEventGroupSetBits(route_eventgroup_handle, UPDATE_TASK_START); 
            return;
        }
    }
}












#include "freertos_demo.h"
#include "led.h"
#include "key.h"
#include "usart.h"
#include "delay.h"
#include "FreeRTOS.h"
#include "event_groups.h" 
#include "task.h"
#include "mac.h"
#include "routing.h"
#include "flash.h"
#include "iwdg.h"
/******************************************************************************************************/

/* start_task任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define START_TASK_PRIO 				1
//单位字=4个字节 定义start_task任务申请的堆栈，
//一般刚开始定义时可以往大了定义，FreeRTOS中提供了可以查询任务历史最小剩余堆栈的API函数uxTaskGetStackHighWaterMark，可以根据剩余内存的大小定义任务堆栈的大小
#define START_TASK_STACK_SIZE		128 
TaskHandle_t start_task_handler;//任务句柄
void start_task(void * pvParameters);

/* 调试信息打印任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务 
 */
#define DEBUG_TASK_PRIO 				  4
#define DEBUG_TASK_STACK_SIZE		156 
TaskHandle_t debug_task_handler;//任务句柄
void debug_task(void * pvParameters);
 
/* 数据上报与计时任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define SEND_TIMER_PRIO 				20
#define SEND_TIMER_STACK_SIZE		640 
TaskHandle_t send_timer_handler;//任务句柄
void send_timer(void * pvParameters);
 
/* MAC数据包处理任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define MAC_PACKET_PROCESS_PRIO 				23
#define MAC_PACKET_PROCESS_STACK_SIZE		256 
TaskHandle_t mac_packet_process_handler;//任务句柄
void mac_packet_process(void * pvParameters);

/* ROUTE数据包处理任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define ROUTE_PACKET_PROCESS_PRIO 				24
#define ROUTE_PACKET_PROCESS_STACK_SIZE		256 
TaskHandle_t route_packet_process_handler;//任务句柄
void route_packet_process(void * pvParameters);

/* 入网任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define JOIN_WAN_PRIO 				22
#define JOIN_WAN_STACK_SIZE		256 
TaskHandle_t join_wan_handler;//任务句柄
void join_wan(void * pvParameters);

/* 本节点地址写入任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define WRITE_MY_ADDR_PRIO 				  26
#define WRITE_MY_ADDR_STACK_SIZE		128 
TaskHandle_t write_my_addr_handler;//任务句柄
void write_my_addr(void * pvParameters);

/* Beacon发送任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define BEACON_SEND_PRIO 					19
#define BEACON_SEND_STACK_SIZE		256 
TaskHandle_t beacon_send_handler;//任务句柄
void beacon_send(void * pvParameters);

/* 数据转发任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define DATA_RELAY_PRIO 				21
#define DATA_RELAY_STACK_SIZE		256 
TaskHandle_t data_relay_handler;//任务句柄
void data_relay(void * pvParameters);

/* 路由更新发送任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define ROUTING_UPDATE_PRIO 				18
#define ROUTING_UPDATE_STACK_SIZE		256 
TaskHandle_t routing_update_handler;//任务句柄
void routing_update(void * pvParameters);

/* 邻居节点与子节点检查任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define NODE_CHECK_PRIO 				15
#define NODE_CHECK_STACK_SIZE		128 
TaskHandle_t node_check_handler;//任务句柄
void node_check(void * pvParameters);

/* 喂狗任务配置
 * 包括：任务句柄 任务优先级 堆栈大小 创建任务
 */
#define FEED_DOG_PRIO 				31
#define FEED_DOG_STACK_SIZE		128 
TaskHandle_t feed_dog_handler;//任务句柄
void feed_dog(void * pvParameters);

/***************************  各任务和定时器周期配置 *********************************/
#define send_timer_period_ms         3600*1000          //数据上报与计时任务周期，单位ms
#define node_check_period_ms         4800*1000          //邻居节点与子节点检查任务
#define feed_dog_period_ms           4*1000             //喂狗任务周期，单位ms
#define debug_task_period_ms         600*1000           //调试信息打印任务周期，单位ms
#define beacon_send_period_ms        900*1000           //Beacon发送任务周期，单位ms
#define reset_recv_ms                40*1000            //强制重置接收方标志，单位ms
#define wait_comm_period_ms          20*1000            //强制休眠时间，单位ms



/******************************************************************************************************/
void Send_Timer_Callback( TimerHandle_t pxTimer );	//发送数据定时器回调函数，超时则发送一次数据
TimerHandle_t send_timer_handle = 0;								/* 单次定时器 */

void Wait_Comm_Timer_Callback( TimerHandle_t pxTimer );	//接收到其他节点的RTS/CTS，等待其通信的定时器回调函数，超时则发送一次数据
TimerHandle_t wait_comm_timer_handle = 0;							  /* 单次定时器 */

void Reset_Recv_Timer_Callback( TimerHandle_t pxTimer );//接收到RTS并成为接收方后，过一段时间自动还原为非接收方，防止卡死在接收RTS后没有接收到数据型帧的状态
TimerHandle_t reset_recv_timer_handle = 0;							/* 单次定时器 */

void Beacon_Send_Timer_Callback( TimerHandle_t pxTimer );//接收到RTS并成为接收方后，过一段时间自动还原为非接收方，防止卡死在接收RTS后没有接收到数据型帧的状态
TimerHandle_t beacon_send_timer_handle = 0;							 /* 单次定时器 */

void Reset_Timer_Callback( TimerHandle_t pxTimer );//重启定时器，每36小时重启一次
TimerHandle_t reset_timer_handle = 0;							 /* 单次定时器 */

extern EventGroupHandle_t recv_eventgroup_handle;		  //接收事件标志组句柄
extern EventBits_t recv_eventgroup_bit;
extern EventGroupHandle_t route_eventgroup_handle;		//路由层事件标志组句柄
extern EventBits_t route_eventgroup_bit;
extern RoutingTable routing_table;                    //定义路由表

extern NodeAddr ADDR_CURRENT;		
extern NodeAddr ADDR_MINE;		
extern MACframe send_frame;											      //发送数据帧
/**
  * @brief  进入Tickless低功耗模式前执行的操作，包括关闭外设时钟、外设供电等
  * @param  None
  * @retval None
  */
void PRE_SLEEP_PROCESSING(void)
{
		
}


/**
  * @brief  退出Tickless低功耗模式后执行的操作,包括恢复外设时钟、外设供电等
  * @param  None
  * @retval None
  */
void POST_SLEEP_PROCESSING(void)
{
		
}

/**
  * @brief  FreeRTOS例程入口函数
  * @param  None
  * @retval None
  */
void freertos_demo(void)
{
		xTaskCreate((TaskFunction_t    		  )   start_task,
								(char *                 )   "start_task",
								(configSTACK_DEPTH_TYPE )   START_TASK_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   START_TASK_PRIO,
								(TaskHandle_t *         )   &start_task_handler );
		vTaskStartScheduler();//开启任务调度
}


/**
  * @brief  start_task任务配置，用于创建任务，创建完后删除自身
  * @param  None
  * @retval None
  */
void start_task(void * pvParameters)
{
    taskENTER_CRITICAL();              /* 进入临界区 */
    if(recv_eventgroup_handle == NULL)
        recv_eventgroup_handle = xEventGroupCreate();
		if(recv_eventgroup_handle != NULL)
		{
				printf("接收事件标志组创造成功!!\r\n");
		}
    
    if(route_eventgroup_handle == NULL)
        route_eventgroup_handle = xEventGroupCreate();
		if(route_eventgroup_handle != NULL)
		{
				printf("路由层事件标志组创造成功!!\r\n");
		}
		xEventGroupSetBits(recv_eventgroup_handle, IS_SENDER | IS_RECEIVER | WAIT_FOR_COMM);  //将IS_SENDER、IS_RECEIVER、WAIT_FOR_COMM三个状态位初始化为1
    xEventGroupSetBits(route_eventgroup_handle, IS_JOIN_WAN );     //将IS_JOIN_WAN位置1，表示未入网；将IS_ADDR_NULL置1，表示当前没有地址
    
		/* 单次定时器 */
		send_timer_handle =  xTimerCreate("send_timer", send_timer_period_ms, pdFALSE, (void *)1, Send_Timer_Callback);
		wait_comm_timer_handle = xTimerCreate("wait_comm_timer", wait_comm_period_ms, pdFALSE, (void *)2, Wait_Comm_Timer_Callback); // 强制休眠时间暂定60s
		reset_recv_timer_handle = xTimerCreate("reset_recv_timer", reset_recv_ms, pdFALSE, (void *)3, Reset_Recv_Timer_Callback); // 强制重置接收方标志位定时器
#if IS_GATWAY	
    beacon_send_timer_handle = xTimerCreate("beacon_send_timer", beacon_send_period_ms, pdFALSE, (void *)4, Beacon_Send_Timer_Callback);
    //reset_timer_handle = xTimerCreate("reset_timer", 129600000, pdFALSE, (void *)5, Reset_Timer_Callback); // 重启定时器
#else
    beacon_send_timer_handle = xTimerCreate("beacon_send_timer", 900000, pdFALSE, (void *)1, Beacon_Send_Timer_Callback);
    //reset_timer_handle = xTimerCreate("reset_timer", 129600000, pdFALSE, (void *)5, Reset_Timer_Callback); // 重启定时器
#endif    
    if(wait_comm_timer_handle != NULL)
		{
				printf("定时器创造成功!!\r\n");
		}
    RoutingTableInitial(routing_table); //初始化路由表
    if((routing_table.node_addr.addr == NULL) || (routing_table.node_addr.addr & 0xffffffffffffffff))  //当前没有地址或地址全为f，则将标志位置1,若不然置0
    {
        xEventGroupSetBits(route_eventgroup_handle, IS_ADDR_NULL); 
    }
    else
    {
        xEventGroupClearBits(route_eventgroup_handle, IS_ADDR_NULL);
    }
    IWDG_Feed();
    xTaskCreate((TaskFunction_t 				)   feed_dog,
								(char *                 )   "feed_dog",
								(configSTACK_DEPTH_TYPE )   FEED_DOG_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   FEED_DOG_PRIO,
								(TaskHandle_t *         )   &feed_dog_handler );

		xTaskCreate((TaskFunction_t 				)   send_timer,
								(char *                 )   "send_timer",
								(configSTACK_DEPTH_TYPE )   SEND_TIMER_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   SEND_TIMER_PRIO,
								(TaskHandle_t *         )   &send_timer_handler );

		xTaskCreate((TaskFunction_t 				)   mac_packet_process,
								(char *                 )   "mac_packet_process",
								(configSTACK_DEPTH_TYPE )   MAC_PACKET_PROCESS_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   MAC_PACKET_PROCESS_PRIO,                                      
								(TaskHandle_t *         )   &mac_packet_process_handler );
                
    xTaskCreate((TaskFunction_t 				)   route_packet_process,
								(char *                 )   "route_packet_process",
								(configSTACK_DEPTH_TYPE )   ROUTE_PACKET_PROCESS_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   ROUTE_PACKET_PROCESS_PRIO,                                      
								(TaskHandle_t *         )   &route_packet_process_handler );
                
    xTaskCreate((TaskFunction_t 				)   beacon_send,
								(char *                 )   "beacon_send",
								(configSTACK_DEPTH_TYPE )   BEACON_SEND_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   BEACON_SEND_PRIO,                                      
								(TaskHandle_t *         )   &beacon_send_handler );
                
    xTaskCreate((TaskFunction_t 				)   join_wan,
								(char *                 )   "join_wan",
								(configSTACK_DEPTH_TYPE )   JOIN_WAN_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   JOIN_WAN_PRIO,                                      
								(TaskHandle_t *         )   &join_wan_handler );
                
    xTaskCreate((TaskFunction_t 				)   write_my_addr,
								(char *                 )   "write_my_addr",
								(configSTACK_DEPTH_TYPE )   WRITE_MY_ADDR_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   WRITE_MY_ADDR_PRIO,                                      
								(TaskHandle_t *         )   &write_my_addr_handler );
                
    xTaskCreate((TaskFunction_t 				)   data_relay,
								(char *                 )   "data_relay",
								(configSTACK_DEPTH_TYPE )   DATA_RELAY_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   DATA_RELAY_PRIO,                                      
								(TaskHandle_t *         )   &data_relay_handler );
                
    xTaskCreate((TaskFunction_t 				)   routing_update,
								(char *                 )   "routing_update",
								(configSTACK_DEPTH_TYPE )   ROUTING_UPDATE_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   ROUTING_UPDATE_PRIO,                                      
								(TaskHandle_t *         )   &routing_update_handler );
                
    xTaskCreate((TaskFunction_t 				)   debug_task,
								(char *                 )   "debug_task",
								(configSTACK_DEPTH_TYPE )   DEBUG_TASK_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   DEBUG_TASK_PRIO,                                      
								(TaskHandle_t *         )   &debug_task_handler );   
                
    xTaskCreate((TaskFunction_t 				)   node_check,
								(char *                 )   "node_check",
								(configSTACK_DEPTH_TYPE )   NODE_CHECK_STACK_SIZE,
								(void *                 )   NULL,
								(UBaseType_t            )   NODE_CHECK_PRIO,                                      
								(TaskHandle_t *         )   &node_check_handler );                   
                
		xEventGroupSetBits(recv_eventgroup_handle, INITIAL_OK);//将INITIAL_OK状态位初始化为1，表示初始化成功
    recv_eventgroup_bit = xEventGroupWaitBits(recv_eventgroup_handle, INITIAL_OK, pdFALSE, pdTRUE, 0);	
		vTaskDelete(NULL);//也可以时vTaskDelete(task1_handler)
    taskEXIT_CRITICAL();               /* 退出临界区 */

}

/**
  * @brief  喂狗任务
  * @param  None
  * @retval None
  */
void feed_dog(void * pvParameters)
{
    while(1)
    {
        IWDG_Feed();
        vTaskDelay(feed_dog_period_ms);
    }
}

/**
  * @brief  调试信息打印任务
  * @param  None
  * @retval None
  */
char time_statictic[500];
void debug_task(void * pvParameters)
{
    UBaseType_t uxHighWaterMark;
    u8 i= 0;
		while(1)
		{
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdTRUE, 0);	//判断是否入网,IS_JOIN_WAN位为1表示未入网
        if(route_eventgroup_bit & IS_JOIN_WAN)
        {
            printf("未入网\r\n");
        }
        else
        {
            printf("已入网\r\n");
        }
        vTaskGetRunTimeStats(time_statictic);//需要配置三个宏，实际上是四个宏，有一个宏是自动调用
        printf("%s\r\n",time_statictic);		 //打印任务运行时间统计
        uxHighWaterMark = uxTaskGetStackHighWaterMark(join_wan_handler); 
        printf("join_wan任务使用情况：%ld\r\n",uxHighWaterMark); 
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark(route_packet_process_handler); 
        printf("route_packet_process任务使用情况：%ld\r\n",uxHighWaterMark); 
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark(write_my_addr_handler); 
        printf("write_my_addr任务使用情况：%ld\r\n",uxHighWaterMark); 
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark(mac_packet_process_handler); 
        printf("mac_packet_process任务使用情况：%ld\r\n",uxHighWaterMark);
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark(routing_update_handler); 
        printf("routing_update任务使用情况：%ld\r\n",uxHighWaterMark);
        uxHighWaterMark = uxTaskGetStackHighWaterMark(send_timer_handler); 
        printf("send_timer任务使用情况：%ld\r\n",uxHighWaterMark);
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark(node_check_handler); 
        printf("node_check任务使用情况：%ld\r\n",uxHighWaterMark);
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark(beacon_send_handler); 
        printf("beacon_send任务使用情况：%ld\r\n",uxHighWaterMark); 
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark(data_relay_handler); 
        printf("data_relay任务使用情况：%ld\r\n",uxHighWaterMark);
        
        uxHighWaterMark = uxTaskGetStackHighWaterMark(feed_dog_handler); 
        printf("feed_dog_handler任务使用情况：%ld\r\n",uxHighWaterMark);
        
        printf("最小剩余栈空间大小 %d \r\n",(int32_t)uxTaskGetStackHighWaterMark(NULL));
        printf("历史剩余最小内存大小:%d 字节\r\n\r\n",xPortGetMinimumEverFreeHeapSize());//查询历史剩余最小内存大小
        printf("当前父节点：%llx\r\n", routing_table.parent_addr);
        printf("当前地址：%llx\r\n", routing_table.tree_pointer->addr);
        printf("当前邻居有：");
        for(i =0; i < routing_table.neighbor_count; i ++)
        {
            printf("%llx, ", routing_table.neighbors[i].addr);
        }
        printf("\r\n\r\n");
        printf("当前子节点有：\r\n");
        TraverseTree(routing_table.tree_pointer);
        printf("\r\n\r\n");
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL); 
        printf("debug_task任务使用情况：%ld\r\n",uxHighWaterMark);
        vTaskDelay(120000 * 10);
		}
}

/**
  * @brief  邻居节点与子节点检查任务配置
  * @param  None
  * @retval None
  */
void node_check(void * pvParameters)
{
    u8 i;
		while(1)
		{
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdTRUE, 0);	//判断是否入网,IS_JOIN_WAN位为1表示未入网
        if(!(route_eventgroup_bit & IS_JOIN_WAN))
        {
            printf("检查邻居节点与子节点任务启动！\r\n");
            node_check_route();
            ReSetNode(routing_table.tree_pointer);
            for(i = 0; i < routing_table.neighbor_count; i++)
            {
                routing_table.neighbors[i].is_neighbors_alive = 0;
            }
        }
        vTaskDelay(node_check_period_ms);
		}
}

/**
  * @brief  定时器任务，每次超时会启动数据上报，在该函数中完成数据定时上报
  * @param  None
  * @retval None
  */
void send_timer(void * pvParameters)
{
		while(1)
		{
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdTRUE, 0);	//判断是否入网,IS_JOIN_WAN位为1表示未入网
        if(!(route_eventgroup_bit & IS_JOIN_WAN))
        {
            printf("上报数据任务启动！\r\n");
            data_report_route();
        }
        vTaskDelay(10);
		}
}

/**
  * @brief  数据转发任务，转发来自子节点的数据
  * @param  None
  * @retval None
  */
void data_relay(void * pvParameters)
{
    while(1)
		{
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdTRUE, 0);	//判断是否入网,IS_JOIN_WAN位为1表示未入网
        if(!(route_eventgroup_bit & IS_JOIN_WAN))
        {
            data_relay_route();
        }
        vTaskDelay(10);
		}
}

/**
  * @brief  路由更新发送任务
  * @param  None
  * @retval None
  */
void routing_update(void * pvParameters)
{
    while(1)
		{
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdTRUE, 0);	//判断是否入网,IS_JOIN_WAN位为1表示未入网
        if(!(route_eventgroup_bit & IS_JOIN_WAN))
        {
            update_broadcast_route();
        }
        vTaskDelay(10);
		}
}


/**
  * @brief  MAC层数据包处理任务
  * @param  None
  * @retval None
  */
void mac_packet_process(void * pvParameters)
{
    packet_process_mac();	
}

/**
  * @brief  路由层数据包处理任务
  * @param  None
  * @retval None
  */
void route_packet_process(void * pvParameters)
{
    packet_process_route();	
}

/**
  * @brief  Beacon发送任务
  * @param  None
  * @retval None
  */
void beacon_send(void * pvParameters)
{
    while(1)
    {
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdTRUE, 0);	//判断是否入网,IS_JOIN_WAN位为1表示未入网
        if(!(route_eventgroup_bit & IS_JOIN_WAN))
        {
            printf("Beacon发送任务启动！\r\n");
            beacon_send_route();
        }
        vTaskDelay(100);	
    }
}

/**
  * @brief  入网任务
  * @param  None
  * @retval None
  */
void join_wan(void * pvParameters)
{
    u8 number_neighbor, rssi_neighbor;
    TreeNode* result = NULL;
    while(1)
    {
//        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_ADDR_NULL, pdFALSE, pdFALSE, 0);
        route_eventgroup_bit = xEventGroupWaitBits(route_eventgroup_handle, IS_JOIN_WAN, pdFALSE, pdFALSE, portMAX_DELAY);
        if((!(route_eventgroup_bit & IS_ADDR_NULL)) && (route_eventgroup_bit & IS_JOIN_WAN)) //如果IS_ADDR_NULL位为0且IS_JOIN_WAN位为1，则表示当前有地址且未入网，需要执行join_wan_route()
            join_wan_route(&number_neighbor, &rssi_neighbor, result);	
        vTaskDelay(500);
    }
}

/**
  * @brief  地址写入任务
  * @param  None
  * @retval None
  */
void write_my_addr(void * pvParameters)
{
    write_my_addr_route();
}


/**
  * @brief  发送数据定时器的超时回调函数
  * @param  pxTimer
  * @retval None
  */
void Send_Timer_Callback( TimerHandle_t pxTimer )
{
		printf("定时器时间到\r\n");
    xEventGroupSetBits(route_eventgroup_handle, TIMER_OK_4);	/* 将事件标志组bit4置1 */
}


/**
  * @brief  强制休眠定时器的超时回调函数
  * @param  pxTimer
  * @retval None
  */
void Wait_Comm_Timer_Callback( TimerHandle_t pxTimer )
{
		printf("强制休眠时间到\r\n");
		xEventGroupSetBits(recv_eventgroup_handle, WAIT_FOR_COMM);	/* 将事件标志组WAIT_FOR_COMM位置1 */

}

/**
  * @brief  重置接收状态定时器的超时回调函数
  * @param  pxTimer
  * @retval None
  */
void Reset_Recv_Timer_Callback( TimerHandle_t pxTimer )
{
		printf("重置接收状态时间到\r\n");
		xEventGroupSetBits(recv_eventgroup_handle, IS_RECEIVER);	/* 将事件标志IS_RECEIVER置1 */
    ADDR_CURRENT = 0;
}

/**
  * @brief  发送Beacon定时器的超时回调函数
  * @param  pxTimer
  * @retval None
  */
void Beacon_Send_Timer_Callback( TimerHandle_t pxTimer )
{
    printf("Beacon发送时间到\r\n");
    xEventGroupSetBits(route_eventgroup_handle, BEACON_TIMER_OK);	/* 将事件标志组BEACON_TIMER_OK置1 */
}

/**
  * @brief  重启定时器的超时回调函数
  *         每36小时重启一次单片机，防止长时间运行后出现问题
  *         该函数会关闭总中断并请求单片机重启   
  * @param  pxTimer
  * @retval None
  */
void Reset_Timer_Callback(TimerHandle_t pxTimer){
    // printf("重启定时器时间到\r\n");
    // __set_FAULTMASK(1); //关闭总中断
    // NVIC_SystemReset(); //请求单片机重启
}

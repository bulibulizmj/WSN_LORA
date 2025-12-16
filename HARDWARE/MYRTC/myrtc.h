#ifndef _MYRTC__H_
#define _MYRTC__H_
 
#define WAKE_UP_SECONDS 3600 //(WAKE_UP_SECONDS * target_overflows)??????


ErrorStatus RTC_Set_Time(u8 hour,u8 min,u8 sec,u8 ampm);
ErrorStatus RTC_Set_Date(u8 year,u8 month,u8 date,u8 week);
u8 My_RTC_Init(void);
void RTC_Set_AlarmA(u8 week,u8 hour,u8 min,u8 sec);
void RTC_Set_WakeUp(u32 psr,u16 arr);
void RTC_Alarm_IRQHandler(void);
void RTC_WKUP_IRQHandler(void);
void SYSCLKConfig_STOP(void);
#endif



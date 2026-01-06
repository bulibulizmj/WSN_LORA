#ifndef _MYRTC__H_
#define _MYRTC__H_

#include <stdint.h>
  
#define WAKE_UP_SECONDS 3600 // 单次唤醒周期(秒)，总重启周期 = WAKE_UP_SECONDS * target_overflows


ErrorStatus RTC_Set_Time(u8 hour,u8 min,u8 sec,u8 ampm);
ErrorStatus RTC_Set_Date(u8 year,u8 month,u8 date,u8 week);
u8 My_RTC_Init(void);
void RTC_Set_AlarmA(u8 week,u8 hour,u8 min,u8 sec);
void RTC_Set_WakeUp(u32 psr,u16 arr);
void RTC_Alarm_IRQHandler(void);
void RTC_WKUP_IRQHandler(void);
void SYSCLKConfig_STOP(void);

/* Convert seconds to RTC WUTR value (when using RTC_WakeUpClock_CK_SPRE_16bits). */
uint16_t RTC_WakeUpSecondsToArr(uint32_t seconds);

/* Measured LSI frequency (Hz). Returns 0 if not available. */
uint32_t RTC_GetLSIFrequencyHz(void);
#endif



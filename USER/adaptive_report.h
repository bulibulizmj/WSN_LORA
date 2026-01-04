#ifndef __ADAPTIVE_REPORT_H
#define __ADAPTIVE_REPORT_H

#include <stdint.h>

/*
 * Adaptive report period algorithm (from patent draft):
 * - Factors: E (energy), S (solar), H (hop cost), C (congestion)
 * - Period:  Tcalc = Tmax * (wE*e^(-lambdaE*E) + wS*e^(-lambdaS*S)) + Tmin * H * C
 * - Smooth:  Tsmooth = alpha*Tprev + (1-alpha)*Tcalc
 * - Clamp:   Tmin <= Tfinal <= Tmax
 * - Gradient clamp: 0.5*Tprev <= Tfinal <= 1.5*Tprev
 * - Mode switch by Vbus: normal / save / emergency
 *
 * All key parameters are configurable via #define below.
 */

/* ------------------------ Time Parameters ------------------------ */
#define ADAPT_REPORT_TMIN_MINUTES              30
#define ADAPT_REPORT_TMAX_MINUTES              180

/* Solar sampling interval and 1-hour average window */
#define ADAPT_REPORT_ENV_SAMPLE_MINUTES        15
#define ADAPT_REPORT_ENV_WINDOW_MINUTES        60

/* ------------------------ Sensor / Normalization ------------------------ */
/* Solar radiation sensor full-scale used for normalization (W/m^2). */
#define ADAPT_REPORT_SMAX_WM2                  1800.0f

/* If radiation reading unit is not W/m^2, adjust scale here. */
#define ADAPT_REPORT_RADIATION_SCALE           1.0f

/* ------------------------ Sensor Fallback ------------------------ */
/* If SHT45 read fails, use this temperature (C) for compensation. */
#define ADAPT_REPORT_TEMP_FALLBACK_C           25.0f

/* CurrentRadiation() returns 0xFFFF on error in this project. */
#define ADAPT_REPORT_RADIATION_INVALID_RAW     0xFFFFu

/* If env sensor read fails, push 0 into the window (more conservative). */
#define ADAPT_REPORT_ENV_ERROR_PUSH_ZERO       1

/* Conservative voltage thresholds (mV). Board is 12V supply. */
#define ADAPT_REPORT_VNOM_MV                   12000
#define ADAPT_REPORT_VMIN_MV                   11000

/* ------------------------ Model Parameters ------------------------ */
/* IIR smoothing factor alpha (0.6~0.8 suggested). */
#define ADAPT_REPORT_ALPHA                     0.7f

/* Normal mode parameters */
#define ADAPT_REPORT_W_E_NORMAL                0.6f
#define ADAPT_REPORT_W_S_NORMAL                0.4f
#define ADAPT_REPORT_LAMBDA_E_NORMAL           1.0f
#define ADAPT_REPORT_LAMBDA_S_NORMAL           1.0f

/* Power-save mode parameters */
#define ADAPT_REPORT_W_E_SAVE                  0.8f
#define ADAPT_REPORT_W_S_SAVE                  0.2f
#define ADAPT_REPORT_LAMBDA_E_SAVE             1.5f
#define ADAPT_REPORT_LAMBDA_S_SAVE             1.0f

/* Hop penalty gamma (0.1~0.5 suggested). */
#define ADAPT_REPORT_GAMMA                     0.3f

/* Congestion penalty beta (0.1~0.3 suggested). */
#define ADAPT_REPORT_BETA                      0.2f

/* ------------------------ Constraints ------------------------ */
#define ADAPT_REPORT_E_MIN                     0.05f
#define ADAPT_REPORT_E_MAX                     1.0f

#define ADAPT_REPORT_S_MIN                     0.0f
#define ADAPT_REPORT_S_MAX                     1.0f

/* Adjacent period change limit: -50% / +50% */
#define ADAPT_REPORT_STEP_DOWN_RATIO           0.5f
#define ADAPT_REPORT_STEP_UP_RATIO             1.5f

/* Hop count fallback */
#define ADAPT_REPORT_HOP_COUNT_DEFAULT         4
#define ADAPT_REPORT_HOP_COUNT_MAX             4

/* Congestion window */
#define ADAPT_REPORT_TX_WINDOW_SIZE            100

/* Debug prints */
#define ADAPT_REPORT_DEBUG                     1

/* ------------------------ API ------------------------ */
void AdaptiveReport_Init(void);

/* Env sampling task entry (created in FreeRTOS start_task). */
void AdaptiveReport_EnvTask(void *pvParameters);

/* Protect shared sensor/I2C access (optional but recommended). */
void AdaptiveReport_SensorLock(void);
void AdaptiveReport_SensorUnlock(void);

/* Record last TX result (ok=1 success, ok=0 fail). */
void AdaptiveReport_RecordTxResult(uint8_t ok);

/* Calculate and update the next report period (ms). hop_count is hops to gateway. */
uint32_t AdaptiveReport_GetNextPeriodMs(uint8_t hop_count);

#endif

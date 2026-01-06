#include "adaptive_report.h"

#include <math.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "delay.h"
#include "led.h"
#include "ina226.h"
#include "mac.h"
#include "mdbs_func.h"
#include "sht45.h"

#if (ADAPT_REPORT_ENV_WINDOW_MINUTES % ADAPT_REPORT_ENV_SAMPLE_MINUTES) != 0
#error "ADAPT_REPORT_ENV_WINDOW_MINUTES must be multiple of ADAPT_REPORT_ENV_SAMPLE_MINUTES"
#endif

#define ADAPT_REPORT_ENV_SAMPLES (ADAPT_REPORT_ENV_WINDOW_MINUTES / ADAPT_REPORT_ENV_SAMPLE_MINUTES)

typedef struct
{
    SemaphoreHandle_t sensor_lock;

    float radiation_sum;
    float radiation_samples[ADAPT_REPORT_ENV_SAMPLES];
    uint16_t radiation_count;
    uint16_t radiation_index;

    uint8_t tx_window[ADAPT_REPORT_TX_WINDOW_SIZE];
    uint16_t tx_count;
    uint16_t tx_index;
    uint16_t tx_fail_count;

    uint32_t prev_period_ms;
} AdaptiveReportState;

static AdaptiveReportState g_adapt;

#if ADAPT_REPORT_SENSOR_TESTSEQ_ENABLE
typedef struct
{
    float temperature_c;
    float humidity;
    uint16_t radiation_raw;
} AdaptiveReportTestSample;

static const AdaptiveReportTestSample g_adapt_test_seq[] =
{
    /* temperature(C), humidity(%RH), radiation(W/m^2) */
    { 15.0f, 70.0f,    0u },
    { 18.0f, 65.0f,  100u },
    { 22.0f, 60.0f,  300u },
    { 26.0f, 55.0f,  600u },
    { 30.0f, 50.0f,  900u },
    { 34.0f, 45.0f, 1200u },
    { 36.0f, 40.0f, 1500u },
    { 35.0f, 40.0f, 1800u },
    { 32.0f, 45.0f, 1200u },
    { 28.0f, 50.0f,  600u },
    { 24.0f, 55.0f,  300u },
    { 20.0f, 60.0f,  100u },
};

static uint16_t g_adapt_test_seq_index = 0;

static float adapt_test_clampf(float v, float vmin, float vmax)
{
    if (v < vmin)
    {
        return vmin;
    }
    if (v > vmax)
    {
        return vmax;
    }
    return v;
}

void AdaptiveReport_TestSeqNext(float *temperature_c, float *humidity, uint16_t *radiation_raw)
{
    const uint16_t seq_len = (uint16_t)(sizeof(g_adapt_test_seq) / sizeof(g_adapt_test_seq[0]));
    AdaptiveReportTestSample sample;

    if (seq_len == 0)
    {
        if (temperature_c != NULL)
        {
            *temperature_c = ADAPT_REPORT_TEMP_FALLBACK_C;
        }
        if (humidity != NULL)
        {
            *humidity = 50.0f;
        }
        if (radiation_raw != NULL)
        {
            *radiation_raw = 0u;
        }
        return;
    }

    if (g_adapt_test_seq_index >= seq_len)
    {
        g_adapt_test_seq_index = 0;
    }

    sample = g_adapt_test_seq[g_adapt_test_seq_index];
    g_adapt_test_seq_index++;
    if (g_adapt_test_seq_index >= seq_len)
    {
        g_adapt_test_seq_index = 0;
    }

    if (temperature_c != NULL)
    {
        *temperature_c = sample.temperature_c;
    }
    if (humidity != NULL)
    {
        *humidity = sample.humidity;
    }
    if (radiation_raw != NULL)
    {
        *radiation_raw = sample.radiation_raw;
    }
}

uint16_t AdaptiveReport_TestSeqTempCToSht45Raw(float temperature_c)
{
    float raw_f = 0.0f;

    temperature_c = adapt_test_clampf(temperature_c, -45.0f, 130.0f);
    raw_f = (temperature_c + 45.0f) * 65535.0f / 175.0f;
    raw_f = adapt_test_clampf(raw_f, 0.0f, 65535.0f);

    return (uint16_t)(raw_f + 0.5f);
}

uint16_t AdaptiveReport_TestSeqHumidityToSht45Raw(float humidity)
{
    float raw_f = 0.0f;

    humidity = adapt_test_clampf(humidity, 0.0f, 100.0f);
    raw_f = (humidity + 6.0f) * 65535.0f / 125.0f;
    raw_f = adapt_test_clampf(raw_f, 0.0f, 65535.0f);

    return (uint16_t)(raw_f + 0.5f);
}
#endif

static void AdaptiveReport_EnsureSensorLockCreated(void)
{
    SemaphoreHandle_t lock = NULL;

    if (g_adapt.sensor_lock != NULL)
    {
        return;
    }

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        return;
    }

    lock = xSemaphoreCreateMutex();
    if (lock == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    if (g_adapt.sensor_lock == NULL)
    {
        g_adapt.sensor_lock = lock;
        lock = NULL;
    }
    taskEXIT_CRITICAL();

    if (lock != NULL)
    {
        vSemaphoreDelete(lock);
    }
}

static float adapt_clampf(float v, float vmin, float vmax)
{
    if (v < vmin)
    {
        return vmin;
    }
    if (v > vmax)
    {
        return vmax;
    }
    return v;
}

static uint32_t adapt_clamp_u32(uint32_t v, uint32_t vmin, uint32_t vmax)
{
    if (v < vmin)
    {
        return vmin;
    }
    if (v > vmax)
    {
        return vmax;
    }
    return v;
}

static float adapt_radiation_avg(void)
{
    if (g_adapt.radiation_count == 0)
    {
        return 0.0f;
    }
    return g_adapt.radiation_sum / (float)g_adapt.radiation_count;
}

static void adapt_radiation_push(float l_comp)
{
    if (ADAPT_REPORT_ENV_SAMPLES == 0)
    {
        return;
    }

    if (g_adapt.radiation_count < ADAPT_REPORT_ENV_SAMPLES)
    {
        g_adapt.radiation_samples[g_adapt.radiation_index] = l_comp;
        g_adapt.radiation_sum += l_comp;
        g_adapt.radiation_count++;
    }
    else
    {
        g_adapt.radiation_sum -= g_adapt.radiation_samples[g_adapt.radiation_index];
        g_adapt.radiation_samples[g_adapt.radiation_index] = l_comp;
        g_adapt.radiation_sum += l_comp;
    }

    g_adapt.radiation_index++;
    if (g_adapt.radiation_index >= ADAPT_REPORT_ENV_SAMPLES)
    {
        g_adapt.radiation_index = 0;
    }
}

static void AdaptiveReport_SampleEnvOnce(void)
{
    float temperature_c = ADAPT_REPORT_TEMP_FALLBACK_C;
    float humidity = 0.0f;
    uint8_t temp_ok = 0;
    uint16_t radiation_raw = 0;
    float radiation_wm2 = 0.0f;
    float l_comp = 0.0f;
    float temp_coeff = 1.0f;

    AdaptiveReport_SensorLock();

#if ADAPT_REPORT_SENSOR_TESTSEQ_ENABLE
    AdaptiveReport_TestSeqNext(&temperature_c, &humidity, &radiation_raw);
    temp_ok = 1;
    (void)humidity;
#else
    sensor_power_on();
    delay_ms(50);

    sht45init();
    temp_ok = (SHT45_ReadPdata(1, &temperature_c, &humidity) == 0) ? 1 : 0;
    (void)humidity;

    radiation_raw = CurrentRadiation();

    sensor_power_off();
#endif

    AdaptiveReport_SensorUnlock();

    if (radiation_raw == (uint16_t)ADAPT_REPORT_RADIATION_INVALID_RAW)
    {
#if ADAPT_REPORT_ENV_ERROR_PUSH_ZERO
        adapt_radiation_push(0.0f);
#endif
        return;
    }

    radiation_wm2 = (float)radiation_raw * ADAPT_REPORT_RADIATION_SCALE;

    /* Lcomp = L * [1 - (T - 25) * 0.004] */
    if (temp_ok)
    {
        temp_coeff = 1.0f - (temperature_c - 25.0f) * 0.004f;
    }
    else
    {
        temp_coeff = 1.0f;
    }
    if (temp_coeff < 0.0f)
    {
        temp_coeff = 0.0f;
    }
    l_comp = radiation_wm2 * temp_coeff;
    if (l_comp < 0.0f)
    {
        l_comp = 0.0f;
    }

    adapt_radiation_push(l_comp);
}

void AdaptiveReport_EnvTask(void *pvParameters)
{
    (void)pvParameters;

    /* First sample immediately after scheduler start. */
    AdaptiveReport_SampleEnvOnce();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS((uint32_t)ADAPT_REPORT_ENV_SAMPLE_MINUTES * 60U * 1000U));
        AdaptiveReport_SampleEnvOnce();
    }
}

void AdaptiveReport_Init(void)
{
#if ADAPT_REPORT_SENSOR_TESTSEQ_ENABLE
    g_adapt_test_seq_index = 0;
#endif
    g_adapt.radiation_sum = 0.0f;
    g_adapt.radiation_count = 0;
    g_adapt.radiation_index = 0;

    g_adapt.tx_count = 0;
    g_adapt.tx_index = 0;
    g_adapt.tx_fail_count = 0;

    g_adapt.prev_period_ms = (uint32_t)ADAPT_REPORT_TMIN_MINUTES * 60U * 1000U;
}

void AdaptiveReport_SensorLock(void)
{
    AdaptiveReport_EnsureSensorLockCreated();
    if (g_adapt.sensor_lock != NULL)
    {
        (void)xSemaphoreTake(g_adapt.sensor_lock, portMAX_DELAY);
    }
}

void AdaptiveReport_SensorUnlock(void)
{
    if (g_adapt.sensor_lock != NULL)
    {
        (void)xSemaphoreGive(g_adapt.sensor_lock);
    }
}

void AdaptiveReport_RecordTxResult(uint8_t ok)
{
    uint8_t old = 0;

    if (ADAPT_REPORT_TX_WINDOW_SIZE == 0)
    {
        return;
    }

    if (g_adapt.tx_count < ADAPT_REPORT_TX_WINDOW_SIZE)
    {
        g_adapt.tx_window[g_adapt.tx_index] = (ok != 0) ? 1 : 0;
        if (ok == 0)
        {
            g_adapt.tx_fail_count++;
        }
        g_adapt.tx_count++;
    }
    else
    {
        old = g_adapt.tx_window[g_adapt.tx_index];
        if (old == 0)
        {
            g_adapt.tx_fail_count--;
        }

        g_adapt.tx_window[g_adapt.tx_index] = (ok != 0) ? 1 : 0;
        if (ok == 0)
        {
            g_adapt.tx_fail_count++;
        }
    }

    g_adapt.tx_index++;
    if (g_adapt.tx_index >= ADAPT_REPORT_TX_WINDOW_SIZE)
    {
        g_adapt.tx_index = 0;
    }
}

uint32_t AdaptiveReport_GetNextPeriodMs(uint8_t hop_count)
{
    int32_t bus_mV = 0;
    uint8_t err = 0;

    uint32_t prev_ms = 0;
    float E = 1.0f;
    float S = 0.0f;
    float H = 1.0f;
    float C = 1.0f;
    float csma_busy_rate = 0.0f;

    float wE = ADAPT_REPORT_W_E_NORMAL;
    float wS = ADAPT_REPORT_W_S_NORMAL;
    float lambdaE = ADAPT_REPORT_LAMBDA_E_NORMAL;
    float lambdaS = ADAPT_REPORT_LAMBDA_S_NORMAL;

    float exp_part = 0.0f;
    float penalty_part = 1.0f;

    uint32_t Tmin_ms = (uint32_t)ADAPT_REPORT_TMIN_MINUTES * 60U * 1000U;
    uint32_t Tmax_ms = (uint32_t)ADAPT_REPORT_TMAX_MINUTES * 60U * 1000U;

    float Tcalc_ms = 0.0f;
    float Tsmooth_ms = 0.0f;
    uint32_t Tsmooth_u32 = 0;
    uint32_t Tafter_hard1_ms = 0;
    uint32_t Tafter_grad_ms = 0;
    uint32_t Tafter_hard2_ms = 0;
    uint32_t down_ms = 0;
    uint32_t up_ms = 0;
    uint8_t clamp_hard = 0;
    uint8_t clamp_grad = 0;
    uint8_t clamp_final = 0;
    uint32_t Tfinal_ms = Tmin_ms;

    uint32_t vdiff_mV = 0;
    uint32_t vth_normal_mV = 0;
    uint32_t vth_critical_mV = 0;

    prev_ms = g_adapt.prev_period_ms;

    /* Read bus voltage (INA226) */
    AdaptiveReport_SensorLock();
    err = INA226_ReadBusVoltage_mV(INA226_I2C_ADDR_DEFAULT, &bus_mV);
    AdaptiveReport_SensorUnlock();
    if (err)
    {
        bus_mV = ADAPT_REPORT_VNOM_MV;
    }

    /* Voltage thresholds */
    if (ADAPT_REPORT_VNOM_MV > ADAPT_REPORT_VMIN_MV)
    {
        vdiff_mV = (uint32_t)(ADAPT_REPORT_VNOM_MV - ADAPT_REPORT_VMIN_MV);
    }
    else
    {
        vdiff_mV = 1;
    }
    vth_normal_mV = (uint32_t)ADAPT_REPORT_VMIN_MV + (uint32_t)((float)vdiff_mV * 0.7f);
    vth_critical_mV = (uint32_t)ADAPT_REPORT_VMIN_MV + (uint32_t)((float)vdiff_mV * 0.2f);

    /* Mode selection */
    if ((uint32_t)bus_mV <= vth_critical_mV)
    {
        g_adapt.prev_period_ms = Tmax_ms;
#if ADAPT_REPORT_DEBUG && ADAPT_REPORT_DEBUG_VERBOSE
        printf("ADAPT_DBG: EMERG prev=%lums Tmin=%lums Tmax=%lums V=%ldmV vthN=%lumV vthC=%lumV ->T=%lums\r\n",
               (unsigned long)prev_ms,
               (unsigned long)Tmin_ms,
               (unsigned long)Tmax_ms,
               (long)bus_mV,
               (unsigned long)vth_normal_mV,
               (unsigned long)vth_critical_mV,
               (unsigned long)Tmax_ms);
#endif
        return Tmax_ms;
    }
    else if ((uint32_t)bus_mV <= vth_normal_mV)
    {
        wE = ADAPT_REPORT_W_E_SAVE;
        wS = ADAPT_REPORT_W_S_SAVE;
        lambdaE = ADAPT_REPORT_LAMBDA_E_SAVE;
        lambdaS = ADAPT_REPORT_LAMBDA_S_SAVE;
    }

    /* E factor */
    if (ADAPT_REPORT_VNOM_MV > ADAPT_REPORT_VMIN_MV)
    {
        E = ((float)bus_mV - (float)ADAPT_REPORT_VMIN_MV) /
            ((float)ADAPT_REPORT_VNOM_MV - (float)ADAPT_REPORT_VMIN_MV);
    }
    else
    {
        E = 1.0f;
    }
    E = adapt_clampf(E, ADAPT_REPORT_E_MIN, ADAPT_REPORT_E_MAX);

    /* S factor (1-hour average of compensated radiation) */
    S = adapt_radiation_avg() / ADAPT_REPORT_SMAX_WM2;
    S = adapt_clampf(S, ADAPT_REPORT_S_MIN, ADAPT_REPORT_S_MAX);

    /* Hop count */
    if ((hop_count == 0xFF) || (hop_count > ADAPT_REPORT_HOP_COUNT_MAX))
    {
        hop_count = ADAPT_REPORT_HOP_COUNT_DEFAULT;
    }
    H = 1.0f + ADAPT_REPORT_GAMMA * (float)hop_count;

    /* Congestion factor from MAC CSMA/CA listen windows (0~1) */
    csma_busy_rate = MAC_GetCsmaCongestionScore();
    csma_busy_rate = adapt_clampf(csma_busy_rate, 0.0f, 1.0f);
    C = 1.0f + ADAPT_REPORT_BETA * csma_busy_rate;

    exp_part = wE * expf(-lambdaE * E) + wS * expf(-lambdaS * S);
    penalty_part = H * C;

    Tcalc_ms = (float)Tmax_ms * exp_part + (float)Tmin_ms * penalty_part;
    Tsmooth_ms = ADAPT_REPORT_ALPHA * (float)prev_ms + (1.0f - ADAPT_REPORT_ALPHA) * Tcalc_ms;

    /* Hard clamp */
    Tsmooth_u32 = (uint32_t)Tsmooth_ms;
    Tafter_hard1_ms = adapt_clamp_u32(Tsmooth_u32, Tmin_ms, Tmax_ms);
    clamp_hard = (Tafter_hard1_ms != Tsmooth_u32) ? 1 : 0;
    Tfinal_ms = Tafter_hard1_ms;

    /* Gradient clamp (+/-50%) */
    down_ms = Tmin_ms;
    up_ms = Tmax_ms;
    Tafter_grad_ms = Tfinal_ms;
    if (prev_ms > 0)
    {
        down_ms = (uint32_t)((float)prev_ms * ADAPT_REPORT_STEP_DOWN_RATIO);
        up_ms = (uint32_t)((float)prev_ms * ADAPT_REPORT_STEP_UP_RATIO);
        if (down_ms < Tmin_ms)
        {
            down_ms = Tmin_ms;
        }
        if (up_ms > Tmax_ms)
        {
            up_ms = Tmax_ms;
        }
        Tafter_grad_ms = adapt_clamp_u32(Tfinal_ms, down_ms, up_ms);
        clamp_grad = (Tafter_grad_ms != Tfinal_ms) ? 1 : 0;
        Tfinal_ms = Tafter_grad_ms;
    }

    /* Final hard clamp */
    Tafter_hard2_ms = adapt_clamp_u32(Tfinal_ms, Tmin_ms, Tmax_ms);
    clamp_final = (Tafter_hard2_ms != Tfinal_ms) ? 1 : 0;
    Tfinal_ms = Tafter_hard2_ms;
    g_adapt.prev_period_ms = Tfinal_ms;

#if ADAPT_REPORT_DEBUG
    printf("ADAPT: V=%ldmV E=%.3f S=%.3f H=%.3f C=%.3f csma=%.3f T=%lums\r\n",
           (long)bus_mV, E, S, H, C, csma_busy_rate, (unsigned long)Tfinal_ms);
#if ADAPT_REPORT_DEBUG_VERBOSE
    printf("ADAPT_DBG: prev=%lums Tmin=%lums Tmax=%lums Tcalc=%.0f Tsmooth=%.0f smooth_u32=%lums hard1=%lums grad=[%lums,%lums] afterGrad=%lums final=%lums ch=%u cg=%u cf=%u\r\n",
           (unsigned long)prev_ms,
           (unsigned long)Tmin_ms,
           (unsigned long)Tmax_ms,
           Tcalc_ms,
           Tsmooth_ms,
           (unsigned long)Tsmooth_u32,
           (unsigned long)Tafter_hard1_ms,
           (unsigned long)down_ms,
           (unsigned long)up_ms,
           (unsigned long)Tafter_grad_ms,
           (unsigned long)Tfinal_ms,
           (unsigned int)clamp_hard,
           (unsigned int)clamp_grad,
           (unsigned int)clamp_final);
#endif
#endif

    return Tfinal_ms;
}

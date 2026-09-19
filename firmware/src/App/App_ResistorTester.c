#include "App_ResistorTester.h"
#include "Com_Time.h"
#include "Driver_ADC.h"
#include <stddef.h>

/* STM32F103 12 位 ADC 的最大码值。 */
#define APP_ADC_FULL_SCALE 4095U

/* 100ms 对应理论 10 次/秒，为原题 >5 次/秒预留调度余量。 */
#define APP_SAMPLE_INTERVAL_MS 100U

/**
 * @brief 一个量程当前最小的软件配置。
 * @note 暂时只有参考电阻参数；GPIO/继电器控制尚未接入。
 */
typedef struct
{
    uint32_t reference_resistor_ohm;
} App_ResistorTesterRangeConfig;

/* 第一版单档验证配置。330Ω 仍是临时值，不代表最终自动量程 BOM。 */
static const App_ResistorTesterRangeConfig s_active_range =
{
    330U
};

static App_ResistorTesterMeasurement s_latest_measurement;
static FunctionalState s_measurement_valid = DISABLE;
static FunctionalState s_sample_started = DISABLE;
static uint32_t s_last_sample_ms = 0U;

/**
 * @brief 将 ADC 原始值按给定参考电阻和分压关系换算为电阻值。
 * @param raw ADC 原始值。
 * @param reference_resistor_ohm 当前量程参考电阻，单位 Ω。
 * @param resistance_ohm 输出电阻值，单位 Ω。
 * @return SUCCESS：换算完成；ERROR：参数无效或 raw=4095 导致分母为 0。
 * @note 拓扑为 VDDA -> Rref -> ADC_MEAS -> Rx -> GND，
 *       因此 Rx = Rref * raw / (4095 - raw)。
 */
static ErrorStatus App_ResistorTester_ConvertRaw(
    uint16_t raw,
    uint32_t reference_resistor_ohm,
    uint32_t *resistance_ohm)
{
    uint32_t denominator;
    uint64_t numerator;

    if ((resistance_ohm == NULL) ||
        (reference_resistor_ohm == 0U) ||
        (raw >= APP_ADC_FULL_SCALE))
    {
        return ERROR;
    }

    denominator = APP_ADC_FULL_SCALE - (uint32_t)raw;
    numerator = (uint64_t)reference_resistor_ohm * (uint64_t)raw;

    /* 加入 denominator/2，实现正整数的四舍五入，而不是直接向下截断。 */
    *resistance_ohm =
        (uint32_t)((numerator + ((uint64_t)denominator / 2U)) /
                   (uint64_t)denominator);

    return SUCCESS;
}

/**
 * @brief 初始化仪器 ADC；时间基准由 main 提前建立。
 */
ErrorStatus App_ResistorTester_Init(void)
{
    s_measurement_valid = DISABLE;
    s_sample_started = DISABLE;
    s_last_sample_ms = 0U;
    s_latest_measurement.adc_raw = 0U;
    s_latest_measurement.resistance_ohm = 0U;
    s_latest_measurement.reference_resistor_ohm = 0U;

    return Driver_ADC1_Init();
}

/**
 * @brief 按 100ms 周期完成一次“ADC 原始值 -> 单档电阻值”更新。
 */
void App_ResistorTester_Task(void)
{
    uint16_t raw;
    uint32_t resistance_ohm;
    const uint32_t now_ms = Com_Time_GetMs();

    if ((s_sample_started == ENABLE) &&
        ((uint32_t)(now_ms - s_last_sample_ms) < APP_SAMPLE_INTERVAL_MS))
    {
        return;
    }

    /* 记录本轮起点，使采样节奏按“开始到开始”计算，不叠加 ADC 执行时间。 */
    s_last_sample_ms = now_ms;
    s_sample_started = ENABLE;

    if (Driver_ADC1_ReadRaw(&raw) != SUCCESS)
    {
        s_measurement_valid = DISABLE;
        return;
    }

    if (App_ResistorTester_ConvertRaw(
            raw,
            s_active_range.reference_resistor_ohm,
            &resistance_ohm) != SUCCESS)
    {
        s_measurement_valid = DISABLE;
        return;
    }

    s_latest_measurement.adc_raw = raw;
    s_latest_measurement.resistance_ohm = resistance_ohm;
    s_latest_measurement.reference_resistor_ohm =
        s_active_range.reference_resistor_ohm;
    s_measurement_valid = ENABLE;
}

/**
 * @brief 返回最近一次有效测量；失败时不覆盖调用者对象。
 */
ErrorStatus App_ResistorTester_GetLatestMeasurement(
    App_ResistorTesterMeasurement *measurement)
{
    if ((measurement == NULL) || (s_measurement_valid != ENABLE))
    {
        return ERROR;
    }

    *measurement = s_latest_measurement;
    return SUCCESS;
}

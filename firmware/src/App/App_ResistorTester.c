#include "App_ResistorTester.h"
#include "Com_Time.h"
#include "Driver_ADC.h"
#include "Interface_Range.h"
#include <stddef.h>

/* STM32F103 12 位 ADC 的最大码值。 */
#define APP_ADC_FULL_SCALE 4095U

/* 100ms 对应理论 10 次/秒，为原题 >5 次/秒预留调度余量。 */
#define APP_SAMPLE_INTERVAL_MS 100U

/* 自动量程候选阈值：20% 下阈值、75% 上阈值。 */
#define APP_RANGE_LOW_THRESHOLD 819U
#define APP_RANGE_HIGH_THRESHOLD 3072U

/**
 * @brief 一个量程当前最小的软件配置。
 * @note 暂时只有参考电阻参数；GPIO/继电器控制尚未接入。
 */
typedef struct
{
    uint32_t reference_resistor_ohm;
} App_ResistorTesterRangeConfig;

/*
 * 前三档当前理论候选值。
 * 33Ω / 330Ω / 3.3kΩ 仍需结合真实硬件误差预算后再定 BOM。
 */
static const App_ResistorTesterRangeConfig s_range_configs[APP_RESISTOR_RANGE_COUNT] =
    {
        [APP_RESISTOR_RANGE_100_OHM] = {33U},
        [APP_RESISTOR_RANGE_1K_OHM] = {330U},
        [APP_RESISTOR_RANGE_10K_OHM] = {3300U} 
        // 等价于下面的写法，但更清晰：
        // s_range_configs[0].reference_resistor_ohm = 33;
        // s_range_configs[1].reference_resistor_ohm = 330;
        // s_range_configs[2].reference_resistor_ohm = 3300;
};

/*
 * 启动阶段会让 Interface_Range 同步选中 1kΩ 档。
 * 自动切换尚未接通，因此运行期间仍固定保持该量程。
 */
static const App_ResistorTesterRange s_active_range = APP_RESISTOR_RANGE_1K_OHM;

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
 * @brief 只根据当前量程与 ADC 值给出下一量程建议。
 * @note 这是纯软件决策，不操作 GPIO，也不改变真实有效量程。
 */
static App_ResistorTesterRange App_ResistorTester_RecommendRange(
    App_ResistorTesterRange current_range,
    uint16_t raw)
{
    if ((raw < APP_RANGE_LOW_THRESHOLD) &&
        (current_range > APP_RESISTOR_RANGE_100_OHM))
    {
        return (App_ResistorTesterRange)((uint32_t)current_range - 1U);
    }

    if ((raw > APP_RANGE_HIGH_THRESHOLD) &&
        (current_range < APP_RESISTOR_RANGE_10K_OHM))
    {
        return (App_ResistorTesterRange)((uint32_t)current_range + 1U);
    }

    return current_range;
}

/**
 * @brief 将 App 的逻辑量程映射到硬件接口量程。
 */
static ErrorStatus App_ResistorTester_MapRangeToInterface(
    App_ResistorTesterRange app_range,
    Interface_Range *interface_range)
{
    if (interface_range == NULL)
    {
        return ERROR;
    }

    switch (app_range)
    {
        case APP_RESISTOR_RANGE_100_OHM:
            *interface_range = INTERFACE_RANGE_100_OHM;
            return SUCCESS;

        case APP_RESISTOR_RANGE_1K_OHM:
            *interface_range = INTERFACE_RANGE_1K_OHM;
            return SUCCESS;

        case APP_RESISTOR_RANGE_10K_OHM:
            *interface_range = INTERFACE_RANGE_10K_OHM;
            return SUCCESS;

        default:
            return ERROR;
    }
}

/**
 * @brief 初始化仪器量程接口与 ADC；时间基准由 main 提前建立。
 */
ErrorStatus App_ResistorTester_Init(void)
{
    s_measurement_valid = DISABLE;
    s_sample_started = DISABLE;
    s_last_sample_ms = 0U;
    s_latest_measurement.adc_raw = 0U;
    s_latest_measurement.resistance_ohm = 0U;
    s_latest_measurement.reference_resistor_ohm = 0U;
    Interface_Range interface_range;

    s_latest_measurement.active_range = s_active_range;
    s_latest_measurement.recommended_range = s_active_range;

    Interface_Range_Init();

    if ((App_ResistorTester_MapRangeToInterface(
             s_active_range,
             &interface_range) != SUCCESS) ||
        (Interface_Range_Select(interface_range) != SUCCESS))
    {
        Interface_Range_DisableAll();
        return ERROR;
    }

    if (Driver_ADC1_Init() != SUCCESS)
    {
        /* ADC 无法工作时，不让继电器继续保持吸合。 */
        Interface_Range_DisableAll();
        return ERROR;
    }

    return SUCCESS;
}

/**
 * @brief 按 100ms 周期完成一次“ADC 原始值 -> 单档电阻值”更新。
 */
void App_ResistorTester_Task(void)
{
    uint16_t raw;
    uint32_t resistance_ohm;
    const uint32_t now_ms = Com_Time_GetMs();
    const App_ResistorTesterRangeConfig *range_config =
        &s_range_configs[s_active_range];

    /* 采样节奏按“开始到开始”计算，若距离上次采样未满 100ms，则直接返回。 */
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
            range_config->reference_resistor_ohm,
            &resistance_ohm) != SUCCESS)
    {
        s_measurement_valid = DISABLE;
        return;
    }

    s_latest_measurement.adc_raw = raw;
    s_latest_measurement.resistance_ohm = resistance_ohm;
    s_latest_measurement.reference_resistor_ohm =
        range_config->reference_resistor_ohm;
    s_latest_measurement.active_range = s_active_range;
    s_latest_measurement.recommended_range =
        App_ResistorTester_RecommendRange(s_active_range, raw);
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

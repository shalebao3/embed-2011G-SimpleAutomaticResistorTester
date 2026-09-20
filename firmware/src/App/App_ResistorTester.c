#include "App_ResistorTester.h"
#include "Com_Time.h"
#include "Driver_ADC.h"
#include "bsp_Range.h"
#include <stddef.h>

/* STM32F103 12 位 ADC 的最大码值。 */
#define APP_ADC_FULL_SCALE 4095U

/* 100ms 对应理论 10 次/秒，为原题 >5 次/秒预留调度余量。 */
#define APP_SAMPLE_INTERVAL_MS 100U

/* 自动量程候选阈值：20% 下阈值、75% 上阈值。 */
#define APP_RANGE_LOW_THRESHOLD 819U
#define APP_RANGE_HIGH_THRESHOLD 3072U

/*
 * 继电器换档的临时软件验证值。
 * 最终必须根据继电器 datasheet 的释放/动作/回跳时间与测量节点 RC 重新确定。
 */
#define APP_RANGE_RELEASE_WAIT_MS 10U
#define APP_RANGE_SETTLE_WAIT_MS 20U

/*
 * 原题要求测量速度 >5 次/s，即稳定结果更新周期必须严格小于 200ms。
 * 三个自动量程之间最多跨两档；以下预算按每次 ADC 都接近驱动超时上限计算，
 * 因而是明显保守于真实 ADC 转换时间的软件上界。
 */
#define APP_REQUIREMENT_MAX_UPDATE_MS 200U
#define APP_MAX_AUTO_RANGE_TRANSITIONS 2U
#define APP_WORST_CASE_UPDATE_BUDGET_MS \
    (APP_SAMPLE_INTERVAL_MS + \
     (APP_MAX_AUTO_RANGE_TRANSITIONS * \
      (APP_RANGE_RELEASE_WAIT_MS + APP_RANGE_SETTLE_WAIT_MS)) + \
     ((APP_MAX_AUTO_RANGE_TRANSITIONS + 1U) * \
      DRIVER_ADC1_READ_TIMEOUT_MS))

_Static_assert(
    APP_WORST_CASE_UPDATE_BUDGET_MS < APP_REQUIREMENT_MAX_UPDATE_MS,
    "Automatic range timing budget must stay below 200ms");

/**
 * @brief 一个量程当前最小的软件配置。
 */
typedef struct
{
    uint32_t reference_resistor_ohm;
} App_ResistorTesterRangeConfig;

/**
 * @brief 非阻塞自动量程状态。
 */
typedef enum
{
    APP_STATE_MEASURE = 0,
    APP_STATE_RANGE_RELEASE_WAIT,
    APP_STATE_RANGE_SETTLE_WAIT,
    APP_STATE_FAULT
} App_ResistorTesterState;

/*
 * 前三档当前理论候选值。
 * 33Ω / 330Ω / 3.3kΩ 仍需结合真实硬件误差预算后再定 BOM。
 */
static const App_ResistorTesterRangeConfig s_range_configs[APP_RESISTOR_RANGE_COUNT] =
{
    [APP_RESISTOR_RANGE_100_OHM] = {33U},
    [APP_RESISTOR_RANGE_1K_OHM] = {330U},
    [APP_RESISTOR_RANGE_10K_OHM] = {3300U}
    /* 等价于：
     * s_range_configs[0].reference_resistor_ohm = 33;
     * s_range_configs[1].reference_resistor_ohm = 330;
     * s_range_configs[2].reference_resistor_ohm = 3300;
     */
};

/*
 * active_range 只表示“已经完成稳定等待、可以用于 ADC 换算”的真实量程。
 * pending_range 表示换档过程中即将接通的目标量程。
 */
static App_ResistorTesterRange s_active_range = APP_RESISTOR_RANGE_1K_OHM;
static App_ResistorTesterRange s_pending_range = APP_RESISTOR_RANGE_1K_OHM;
static App_ResistorTesterState s_state = APP_STATE_MEASURE;
static uint32_t s_state_started_ms = 0U;

static App_ResistorTesterMeasurement s_latest_measurement;
static FunctionalState s_result_available = DISABLE;
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
    uint32_t denominator;  // 分母，计算时必须使用 32 位整数避免溢出
    uint64_t numerator;  // 分子，计算时必须使用 64 位整数避免溢出

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
 * @brief 根据当前量程与 ADC 值给出下一量程建议。
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
 * @brief 将 板级 BSP 量程映射为 App 逻辑量程。
 * @param app_range 期望的 App 逻辑量程。
 * @param bsp_range 输出的 BSP 量程。
 * @return SUCCESS：映射成功；ERROR：参数无效。
 */
static ErrorStatus App_ResistorTester_MapRangeToBsp(
    App_ResistorTesterRange app_range,
    Bsp_Range *bsp_range)
{
    if (bsp_range == NULL)
    {
        return ERROR;
    }

    switch (app_range)
    {
        case APP_RESISTOR_RANGE_100_OHM:
            *bsp_range = BSP_RANGE_100_OHM;
            return SUCCESS;

        case APP_RESISTOR_RANGE_1K_OHM:
            *bsp_range = BSP_RANGE_1K_OHM;
            return SUCCESS;

        case APP_RESISTOR_RANGE_10K_OHM:
            *bsp_range = BSP_RANGE_10K_OHM;
            return SUCCESS;

        default:
            return ERROR;
    }
}

/**
 * @brief 系统进入不可继续工作的状态，先把硬件切到安全状态，再更新软件状态
 */
static void App_ResistorTester_EnterFault(void)
{
    Bsp_Range_DisableAll();
    s_result_available = DISABLE;
    s_latest_measurement.status = APP_MEASUREMENT_STATUS_UNAVAILABLE;
    s_state = APP_STATE_FAULT;
}

/**
 * @brief 初始化 BSP 量程控制与 ADC；时间基准由 main 提前建立。
 */
ErrorStatus App_ResistorTester_Init(void)
{
    Bsp_Range bsp_range; // 临时接住：App_ResistorTester_MapRangeToBsp(...)转换出来的 BSP 量程

    s_active_range = APP_RESISTOR_RANGE_1K_OHM;  // 1kΩ 档为默认初始档
    s_pending_range = s_active_range;  
    s_state = APP_STATE_MEASURE;  // 初始态为测量态，后续会在 Task 中进入换档等待
    s_state_started_ms = 0U;

    s_result_available = DISABLE;
    s_sample_started = DISABLE;
    s_last_sample_ms = 0U;
    s_latest_measurement.adc_raw = 0U;
    s_latest_measurement.resistance_ohm = 0U;
    s_latest_measurement.reference_resistor_ohm = 0U;
    s_latest_measurement.active_range = s_active_range;
    s_latest_measurement.recommended_range = s_active_range;
    s_latest_measurement.status = APP_MEASUREMENT_STATUS_UNAVAILABLE;

    Bsp_Range_Init(); 

    if ((App_ResistorTester_MapRangeToBsp(
                 s_active_range,
                 &bsp_range) != SUCCESS) ||
            (Bsp_Range_Select(bsp_range) != SUCCESS))
    {
        App_ResistorTester_EnterFault();
        return ERROR;
    }

    if (Driver_ADC1_Init() != SUCCESS)
    {
        /* ADC 无法工作时，不让继电器继续保持吸合。 */
        App_ResistorTester_EnterFault();
        return ERROR;
    }

    /*
     * 上电时初始 1kΩ 档刚刚接通，必须等待继电器动作、触点回跳
     * 和模拟节点稳定后，才允许第一次 ADC。
     */
    s_state = APP_STATE_RANGE_SETTLE_WAIT;
    s_state_started_ms = Com_Time_GetMs();

    return SUCCESS;
}

/**
 * @brief 推进非阻塞自动换档状态机。
 * @param now_ms 当前毫秒时基。
 * @return ENABLE：本轮已经由状态机处理，应立即返回；DISABLE：可以继续测量。
 */
static FunctionalState App_ResistorTester_ProcessRangeState(uint32_t now_ms)
{
    Bsp_Range bsp_range;

    switch (s_state)
    {
    case APP_STATE_FAULT:
        return ENABLE;

    case APP_STATE_RANGE_RELEASE_WAIT:
        if ((uint32_t)(now_ms - s_state_started_ms) < APP_RANGE_RELEASE_WAIT_MS)
        {
            return ENABLE;
        }

        if ((App_ResistorTester_MapRangeToBsp(
                 s_pending_range,
                 &bsp_range) != SUCCESS) ||
            (Bsp_Range_Select(bsp_range) != SUCCESS))
        {
            App_ResistorTester_EnterFault();
            return ENABLE;
        }

        s_state = APP_STATE_RANGE_SETTLE_WAIT;
        s_state_started_ms = Com_Time_GetMs();
        return ENABLE;

    case APP_STATE_RANGE_SETTLE_WAIT:
        if ((uint32_t)(now_ms - s_state_started_ms) < APP_RANGE_SETTLE_WAIT_MS)
        {
            return ENABLE;
        }

        /*
         * 到这里才认为新量程已经稳定。
         * 因此 active_range 永远不提前于真实硬件稳定状态。
         */
        s_active_range = s_pending_range;
        s_latest_measurement.active_range = s_active_range;
        s_latest_measurement.recommended_range = s_active_range;
        s_latest_measurement.status = APP_MEASUREMENT_STATUS_UNAVAILABLE;
        s_sample_started = DISABLE;
        s_state = APP_STATE_MEASURE;
        return ENABLE;

    case APP_STATE_MEASURE:
    default:
        /* 测量态不由换档状态机处理，交回给调用方继续测量。 */
        return DISABLE;
    }
}

/**
 * @brief 按 100ms 周期完成测量，并在需要时启动非阻塞自动换档。
 */
void App_ResistorTester_Task(void)
{
    uint16_t raw;
    uint32_t resistance_ohm;
    App_ResistorTesterRange recommended_range;
    const uint32_t now_ms = Com_Time_GetMs();
    const App_ResistorTesterRangeConfig *range_config;

    if (App_ResistorTester_ProcessRangeState(now_ms) == ENABLE)
    {
        return;
    }

    range_config = &s_range_configs[s_active_range];

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
        s_result_available = DISABLE;
        return;
    }

    /*
     * 先判断是否需要换档，再决定是否计算 Rx。
     * 对于即将丢弃的量程结果没有必要做电阻换算；raw=4095 虽然无法进入
     * 分压反算公式，但仍然是明确的“应该向更高量程移动”信号。
     */
    recommended_range =
        App_ResistorTester_RecommendRange(s_active_range, raw);

    if (recommended_range != s_active_range)
    {
        s_result_available = DISABLE;
        s_latest_measurement.status = APP_MEASUREMENT_STATUS_UNAVAILABLE;
        s_pending_range = recommended_range;
        Bsp_Range_DisableAll();
        s_state = APP_STATE_RANGE_RELEASE_WAIT;
        s_state_started_ms = Com_Time_GetMs();
        return;
    }

    /*
     * 10kΩ 是当前最高自动档。若此时 ADC 仍高于上阈值，
     * 不把越界估算值当作有效阻值，而是明确要求进入 10MΩ 高阻档。
     */
    if ((s_active_range == APP_RESISTOR_RANGE_10K_OHM) &&
        (raw > APP_RANGE_HIGH_THRESHOLD))
    {
        s_latest_measurement.adc_raw = raw;
        s_latest_measurement.resistance_ohm = 0U;
        s_latest_measurement.reference_resistor_ohm =
            range_config->reference_resistor_ohm;
        s_latest_measurement.active_range = s_active_range;
        s_latest_measurement.recommended_range = s_active_range;
        s_latest_measurement.status =
            APP_MEASUREMENT_STATUS_HIGH_RANGE_REQUIRED;
        s_result_available = ENABLE;
        return;
    }

    if (App_ResistorTester_ConvertRaw(
            raw,
            range_config->reference_resistor_ohm,
            &resistance_ohm) != SUCCESS)
    {
        s_result_available = DISABLE;
        s_latest_measurement.status = APP_MEASUREMENT_STATUS_UNAVAILABLE;
        return;
    }

    s_latest_measurement.adc_raw = raw;
    s_latest_measurement.resistance_ohm = resistance_ohm;
    s_latest_measurement.reference_resistor_ohm =
        range_config->reference_resistor_ohm;
    s_latest_measurement.active_range = s_active_range;
    s_latest_measurement.recommended_range = recommended_range;
    s_latest_measurement.status = APP_MEASUREMENT_STATUS_VALID;
    s_result_available = ENABLE;
}

/**
 * @brief 返回最近一次有效测量；失败时不覆盖调用者对象。
 */
ErrorStatus App_ResistorTester_GetLatestMeasurement(
    App_ResistorTesterMeasurement *measurement)
{
    if ((measurement == NULL) || (s_result_available != ENABLE))
    {
        return ERROR;
    }

    *measurement = s_latest_measurement;
    return SUCCESS;
}

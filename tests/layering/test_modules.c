/* 使用接口替身验证真实 App/Driver 源码，不模拟 ADC 电气行为。 */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "App_ResistorTester.h"
#include "Driver_ADC.h"
#include "Com_Time.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
    exit(EXIT_FAILURE); } } while (0)

GPIO_TypeDef mock_gpio_a;
ADC_TypeDef mock_adc1;
static uint32_t now_ms;
static unsigned reset_polls, cal_polls;
static int reset_stuck, cal_stuck;
/* 只模拟转换握手，不声称覆盖采样电容、参考电压或真实时序。 */
static int adc_enabled, read_stuck;
static unsigned read_polls, ready_after = 2U, conversion_starts;
static FlagStatus eoc;
static uint16_t conversion_input = 2048U, data_register;
static int events[128];
static size_t event_count;

enum {
    CLOCK_ADC, ADC_DIV6, ADC_RESET, GPIO_DEFAULTS,
    GPIO_ANALOG, ADC_DEFAULTS, ADC_SETUP,
    ADC_CHANNEL0, ADC_ENABLE, DELAY_2MS, RESET_CAL, START_CAL,
    ADC_DISABLE, EOC_CLEAR, CONVERSION_START, CONVERSION_READ
};

static void record(int event)
{
    CHECK(event_count < sizeof(events) / sizeof(events[0]));
    events[event_count++] = event;
}

static void expect_events(const int *expected, size_t count)
{
    CHECK(event_count == count);
    for (size_t i = 0; i < count; ++i) { CHECK(events[i] == expected[i]); }
}

uint32_t Com_Time_GetMs(void) { return now_ms++; }
void Com_Time_DelayMs(uint32_t delay_ms)
{
    CHECK(delay_ms == 2U);
    record(DELAY_2MS);
    now_ms += delay_ms;
}

void RCC_APB2PeriphClockCmd(uint32_t peripheral, FunctionalState state)
{
    CHECK(state == ENABLE);
    CHECK(peripheral == (RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1));
    record(CLOCK_ADC);
}
void RCC_ADCCLKConfig(uint32_t divider) { CHECK(divider == RCC_PCLK2_Div6); record(ADC_DIV6); }
void ADC_DeInit(ADC_TypeDef *adc) { CHECK(adc == ADC1); record(ADC_RESET); }
void GPIO_StructInit(GPIO_InitTypeDef *config)
{
    memset(config, 0, sizeof(*config));
    record(GPIO_DEFAULTS);
}
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config)
{
    CHECK(port == GPIOA);
    CHECK(config->GPIO_Pin == GPIO_Pin_0 && config->GPIO_Mode == GPIO_Mode_AIN);
    record(GPIO_ANALOG);
}
void ADC_StructInit(ADC_InitTypeDef *config)
{
    /* 毒化默认值，确认被测代码显式设置了所有六个字段。 */
    memset(config, 0x55, sizeof(*config)); record(ADC_DEFAULTS);
}
void ADC_Init(ADC_TypeDef *adc, ADC_InitTypeDef *config)
{
    CHECK(adc == ADC1);
    CHECK(config->ADC_Mode == ADC_Mode_Independent);
    CHECK(config->ADC_ScanConvMode == DISABLE);
    CHECK(config->ADC_ContinuousConvMode == DISABLE);
    CHECK(config->ADC_ExternalTrigConv == ADC_ExternalTrigConv_None);
    CHECK(config->ADC_DataAlign == ADC_DataAlign_Right);
    CHECK(config->ADC_NbrOfChannel == 1U);
    record(ADC_SETUP);
}
void ADC_RegularChannelConfig(ADC_TypeDef *adc, uint8_t channel, uint8_t rank, uint8_t sample)
{
    CHECK(adc == ADC1 && channel == ADC_Channel_0 && rank == 1U);
    CHECK(sample == ADC_SampleTime_239Cycles5); record(ADC_CHANNEL0);
}
void ADC_Cmd(ADC_TypeDef *adc, FunctionalState state)
{
    CHECK(adc == ADC1);
    adc_enabled = state == ENABLE;
    record(state == ENABLE ? ADC_ENABLE : ADC_DISABLE);
}
void ADC_ResetCalibration(ADC_TypeDef *adc) { CHECK(adc == ADC1); record(RESET_CAL); }
FlagStatus ADC_GetResetCalibrationStatus(ADC_TypeDef *adc)
{
    CHECK(adc == ADC1); CHECK(++reset_polls < 32U);
    return (reset_stuck || reset_polls == 1U) ? SET : RESET;
}
void ADC_StartCalibration(ADC_TypeDef *adc) { CHECK(adc == ADC1); record(START_CAL); }
FlagStatus ADC_GetCalibrationStatus(ADC_TypeDef *adc)
{
    CHECK(adc == ADC1); CHECK(++cal_polls < 32U);
    return (cal_stuck || cal_polls == 1U) ? SET : RESET;
}

void ADC_ClearFlag(ADC_TypeDef *adc, uint8_t flag)
{
    CHECK(adc == ADC1 && flag == ADC_FLAG_EOC);
    eoc = RESET;
    record(EOC_CLEAR);
}
void ADC_SoftwareStartConvCmd(ADC_TypeDef *adc, FunctionalState state)
{
    CHECK(adc == ADC1 && state == ENABLE && adc_enabled);
    /* 上次 EOC 必须先清理；不会在 mock 中替驱动自动修复此条件。 */
    CHECK(eoc == RESET);
    read_polls = 0U;
    conversion_starts++;
    record(CONVERSION_START);
}
FlagStatus ADC_GetFlagStatus(ADC_TypeDef *adc, uint8_t flag)
{
    CHECK(adc == ADC1 && flag == ADC_FLAG_EOC && adc_enabled);
    CHECK(conversion_starts > 0U && ++read_polls < 32U);
    if (!read_stuck && read_polls >= ready_after) {
        data_register = conversion_input;
        eoc = SET;
    }
    return eoc;
}
uint16_t ADC_GetConversionValue(ADC_TypeDef *adc)
{
    CHECK(adc == ADC1 && adc_enabled && eoc == SET);
    eoc = RESET;
    record(CONVERSION_READ);
    return data_register;
}

/* 每个 CTest 用例是独立进程，驱动的私有就绪状态也会重新初始化。 */
static void test_read(const char *name)
{
    uint16_t raw = 0xA55AU; /* 哨兵值：失败不得覆盖输出对象。 */
    if (strcmp(name, "read_before_init") == 0) {
        CHECK(Driver_ADC1_ReadRaw(&raw) == ERROR);
        CHECK(raw == 0xA55AU && event_count == 0U && now_ms == 0U);
        return;
    }

    CHECK(Driver_ADC1_Init() == SUCCESS);
    event_count = 0U;
    if (strcmp(name, "read_null") == 0) {
        const uint32_t before = now_ms;
        CHECK(Driver_ADC1_ReadRaw(NULL) == ERROR);
        CHECK(event_count == 0U && now_ms == before && adc_enabled);
        /* 参数错误不破坏已成功初始化的状态。 */
        CHECK(Driver_ADC1_ReadRaw(&raw) == SUCCESS && raw == 2048U);
        return;
    }
    if (strcmp(name, "read_after_init_failure") == 0) {
        reset_polls = cal_polls = 0U;
        cal_stuck = 1;
        CHECK(Driver_ADC1_Init() == ERROR);
        event_count = 0U;
        CHECK(Driver_ADC1_ReadRaw(&raw) == ERROR);
        CHECK(raw == 0xA55AU && event_count == 0U && !adc_enabled);
        return;
    }

    if (strstr(name, "wrap") != NULL) { now_ms = UINT32_MAX; }
    if (strcmp(name, "read_zero") == 0) { conversion_input = 0U; }
    if (strcmp(name, "read_fullscale") == 0) { conversion_input = 4095U; }
    if (strcmp(name, "read_immediate") == 0) { ready_after = 1U; }
    if (strcmp(name, "read_stale_eoc") == 0) {
        eoc = SET; data_register = 17U; conversion_input = 3000U;
    }
    if (strstr(name, "timeout") != NULL) { read_stuck = 1; }
    const uint32_t before = now_ms;
    const ErrorStatus result = Driver_ADC1_ReadRaw(&raw);
    if (read_stuck) {
        const int expected[] = {EOC_CLEAR, CONVERSION_START, ADC_DISABLE, EOC_CLEAR};
        CHECK(result == ERROR && raw == 0xA55AU);
        CHECK(!adc_enabled && eoc == RESET && read_polls == 10U);
        CHECK((uint32_t)(now_ms - before) == 11U);
        expect_events(expected, sizeof(expected) / sizeof(expected[0]));
        /* 超时后即使冒出一个迟到的 EOC，也不能把旧结果作为新值。 */
        eoc = SET; data_register = 999U;
        CHECK(Driver_ADC1_ReadRaw(&raw) == ERROR && raw == 0xA55AU);
        CHECK(event_count == 4U && conversion_starts == 1U);
        if (strcmp(name, "read_reinit_after_timeout") != 0) { return; }
        reset_polls = cal_polls = 0U;
        read_stuck = 0;
        CHECK(Driver_ADC1_Init() == SUCCESS);
        event_count = 0U;
        CHECK(Driver_ADC1_ReadRaw(&raw) == SUCCESS && raw == 2048U);
        CHECK(adc_enabled && conversion_starts == 2U);
        return;
    }

    const int expected[] = {EOC_CLEAR, CONVERSION_START, CONVERSION_READ};
    CHECK(result == SUCCESS && raw == conversion_input && adc_enabled);
    CHECK(read_polls == ready_after && eoc == RESET && conversion_starts == 1U);
    CHECK((uint32_t)(now_ms - before) == ready_after);
    expect_events(expected, sizeof(expected) / sizeof(expected[0]));
    if (strcmp(name, "read_repeat") == 0) {
        event_count = 0U;
        conversion_input = 1234U;
        CHECK(Driver_ADC1_ReadRaw(&raw) == SUCCESS && raw == 1234U);
        CHECK(conversion_starts == 2U && adc_enabled && eoc == RESET);
        expect_events(expected, sizeof(expected) / sizeof(expected[0]));
    }
}


static void test_app_measurement(const char *name)
{
    App_ResistorTesterMeasurement measurement = {
        0xAAAAU,
        0x55555555U,
        0x33333333U
    };

    CHECK(App_ResistorTester_Init() == SUCCESS);
    event_count = 0U;

    if (strcmp(name, "measure_get_before_sample") == 0) {
        CHECK(App_ResistorTester_GetLatestMeasurement(&measurement) == ERROR);
        CHECK(measurement.adc_raw == 0xAAAAU &&
              measurement.resistance_ohm == 0x55555555U &&
              measurement.reference_resistor_ohm == 0x33333333U);
        CHECK(App_ResistorTester_GetLatestMeasurement(NULL) == ERROR);
        CHECK(conversion_starts == 0U);
        return;
    }

    if (strcmp(name, "measure_1k") == 0) { conversion_input = 3079U; }
    if (strcmp(name, "measure_zero") == 0) { conversion_input = 0U; }
    if (strcmp(name, "measure_fullscale_invalid") == 0) { conversion_input = 4095U; }
    if (strcmp(name, "measure_read_error_invalid") == 0) { read_stuck = 1; }

    App_ResistorTester_Task();

    if (strcmp(name, "measure_fullscale_invalid") == 0) {
        CHECK(conversion_starts == 1U && adc_enabled);
        CHECK(App_ResistorTester_GetLatestMeasurement(&measurement) == ERROR);
        return;
    }

    if (strcmp(name, "measure_read_error_invalid") == 0) {
        CHECK(conversion_starts == 1U && !adc_enabled);
        CHECK(App_ResistorTester_GetLatestMeasurement(&measurement) == ERROR);
        return;
    }

    CHECK(App_ResistorTester_GetLatestMeasurement(&measurement) == SUCCESS);
    CHECK(measurement.adc_raw == conversion_input);
    CHECK(measurement.reference_resistor_ohm == 330U);

    if (strcmp(name, "measure_midscale") == 0) {
        CHECK(measurement.resistance_ohm == 330U);
    } else if (strcmp(name, "measure_1k") == 0) {
        CHECK(measurement.resistance_ohm == 1000U);
    } else if (strcmp(name, "measure_zero") == 0) {
        CHECK(measurement.resistance_ohm == 0U);
    } else if (strcmp(name, "measure_interval") == 0) {
        const unsigned before = conversion_starts;
        App_ResistorTester_Task();
        CHECK(conversion_starts == before);
        now_ms += 100U;
        App_ResistorTester_Task();
        CHECK(conversion_starts == before + 1U);
    }
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *name = argv[1];
    if (strncmp(name, "read_", 5U) == 0) {
        test_read(name);
    } else if (strncmp(name, "measure_", 8U) == 0) {
        test_app_measurement(name);
    } else {
        reset_stuck = strstr(name, "reset_timeout") != NULL;
        cal_stuck = strstr(name, "cal_timeout") != NULL;
        if (strstr(name, "wrap") != NULL) { now_ms = UINT32_MAX - 5U; }
        const int is_app = strncmp(name, "app_", 4U) == 0;
        const ErrorStatus result = is_app ? App_ResistorTester_Init() : Driver_ADC1_Init();
        CHECK(result == ((reset_stuck || cal_stuck) ? ERROR : SUCCESS));
        const int adc_prefix[] = {
            CLOCK_ADC, ADC_DIV6, ADC_RESET, GPIO_DEFAULTS, GPIO_ANALOG,
            ADC_DEFAULTS, ADC_SETUP, ADC_CHANNEL0, ADC_ENABLE, DELAY_2MS, RESET_CAL
        };
        size_t offset = 0U;
        for (size_t i = 0; i < sizeof(adc_prefix) / sizeof(adc_prefix[0]); ++i) {
            CHECK(events[offset++] == adc_prefix[i]);
        }
        if (reset_stuck) {
            CHECK(reset_polls == 10U && cal_polls == 0U);
            CHECK(events[offset++] == ADC_DISABLE);
        } else {
            CHECK(reset_polls == 2U && events[offset++] == START_CAL);
            if (cal_stuck) {
                CHECK(cal_polls == 10U && events[offset++] == ADC_DISABLE);
            } else { CHECK(cal_polls == 2U); }
        }
        CHECK(offset == event_count);
    }
    printf("PASS: %s\n", name);
    return EXIT_SUCCESS;
}

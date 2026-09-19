/* 使用接口替身验证真实 App/Driver/Interface 源码，不模拟 ADC 电气行为。 */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "App_ResistorTester.h"
#include "Driver_ADC.h"
#include "Interface_LED.h"
#include "Com_Time.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #expr); \
    exit(EXIT_FAILURE); } } while (0)

GPIO_TypeDef mock_gpio_a, mock_gpio_c;
ADC_TypeDef mock_adc1;
static uint32_t now_ms;
static unsigned reset_polls, cal_polls;
static int reset_stuck, cal_stuck;
static int events[128];
static size_t event_count;

enum {
    CLOCK_LED, CLOCK_ADC, ADC_DIV6, ADC_RESET, GPIO_DEFAULTS,
    GPIO_LED, GPIO_ANALOG, LED_OFF, LED_ON, ADC_DEFAULTS, ADC_SETUP,
    ADC_CHANNEL0, ADC_ENABLE, DELAY_2MS, RESET_CAL, START_CAL,
    ADC_DISABLE, DELAY_500MS
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
    CHECK(delay_ms == 2U || delay_ms == 500U);
    record(delay_ms == 2U ? DELAY_2MS : DELAY_500MS);
    now_ms += delay_ms;
}

void RCC_APB2PeriphClockCmd(uint32_t peripheral, FunctionalState state)
{
    CHECK(state == ENABLE);
    CHECK(peripheral == RCC_APB2Periph_GPIOC ||
          peripheral == (RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1));
    record(peripheral == RCC_APB2Periph_GPIOC ? CLOCK_LED : CLOCK_ADC);
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
    if (port == GPIOA) {
        CHECK(config->GPIO_Pin == GPIO_Pin_0 && config->GPIO_Mode == GPIO_Mode_AIN);
        record(GPIO_ANALOG);
    } else {
        CHECK(port == GPIOC && config->GPIO_Pin == GPIO_Pin_13);
        CHECK(config->GPIO_Mode == GPIO_Mode_Out_PP && config->GPIO_Speed == GPIO_Speed_2MHz);
        record(GPIO_LED);
    }
}
void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pins)
{
    CHECK(port == GPIOC && pins == GPIO_Pin_13); record(LED_OFF);
}
void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pins)
{
    CHECK(port == GPIOC && pins == GPIO_Pin_13); record(LED_ON);
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
    CHECK(adc == ADC1); record(state == ENABLE ? ADC_ENABLE : ADC_DISABLE);
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

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    const char *name = argv[1];
    if (strcmp(name, "led") == 0) {
        const int expected[] = {CLOCK_LED, GPIO_DEFAULTS, GPIO_LED, LED_OFF, LED_ON, LED_OFF};
        Interface_LED_Init(); Interface_LED_Set(true); Interface_LED_Set(false);
        expect_events(expected, sizeof(expected) / sizeof(expected[0]));
    } else if (strcmp(name, "app_cycle") == 0) {
        const int expected[] = {LED_ON, DELAY_500MS, LED_OFF, DELAY_500MS};
        App_ResistorTester_Task();
        expect_events(expected, sizeof(expected) / sizeof(expected[0]));
        CHECK(now_ms == 1000U);
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
        if (is_app) {
            const int led_prefix[] = {CLOCK_LED, GPIO_DEFAULTS, GPIO_LED, LED_OFF};
            for (size_t i = 0; i < 4U; ++i) { CHECK(events[offset++] == led_prefix[i]); }
        }
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

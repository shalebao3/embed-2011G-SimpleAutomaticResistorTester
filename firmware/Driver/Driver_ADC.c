#include "Driver_ADC.h"
#include "Com_Time.h"

/* 校准等待的软件保护值，单位：毫秒；保持原配置不变。 */
#define ADC_CAL_TIMEOUT_MS 10U

/**
 * @brief 将原 main.c 中的 ADC 初始化等价迁移，保留配置及处理顺序。
 */
ErrorStatus Driver_ADC1_Init(void)
{
    GPIO_InitTypeDef gpio_init; /* PA0 引脚配置。 */
    ADC_InitTypeDef adc_init;   /* ADC 工作方式配置。 */
    uint32_t start_ms;          /* 当前等待阶段的起始时间。 */

    /* 1. 开启 GPIOA 和 ADC1 的外设时钟。 */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1,
        ENABLE);

    /* PCLK2 六分频：正常 PCLK2=72MHz 时，ADCCLK=12MHz。 */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 只复位 ADC1，建立明确的初始状态。 */
    ADC_DeInit(ADC1);

    /* 2. PA0 配置为模拟输入。 */
    GPIO_StructInit(&gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_0;
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio_init);

    /* 3. 配置 ADC1 的工作方式。 */
    ADC_StructInit(&adc_init);

    adc_init.ADC_Mode = ADC_Mode_Independent;
    adc_init.ADC_ScanConvMode = DISABLE;
    adc_init.ADC_ContinuousConvMode = DISABLE;
    adc_init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adc_init.ADC_DataAlign = ADC_DataAlign_Right;
    adc_init.ADC_NbrOfChannel = 1;

    ADC_Init(ADC1, &adc_init);

    /* 4. 规则序列第 1 位采集通道 0，即 PA0。 */
    ADC_RegularChannelConfig(
        ADC1,
        ADC_Channel_0,
        1,
        ADC_SampleTime_239Cycles5);

    /* 5. ADC 上电稳定等待；沿用原来的 2ms。 */
    ADC_Cmd(ADC1, ENABLE);
    Com_Time_DelayMs(2U);

    /* 6. 复位校准寄存器，等待硬件完成。 */
    ADC_ResetCalibration(ADC1);
    start_ms = Com_Time_GetMs();

    while (ADC_GetResetCalibrationStatus(ADC1) != RESET)
    {
        if ((uint32_t)(Com_Time_GetMs() - start_ms) >= ADC_CAL_TIMEOUT_MS)
        {
            ADC_Cmd(ADC1, DISABLE);
            return ERROR;
        }
    }

    /* 7. 启动内部校准，等待硬件完成。 */
    ADC_StartCalibration(ADC1);
    start_ms = Com_Time_GetMs();

    while (ADC_GetCalibrationStatus(ADC1) != RESET)
    {
        if ((uint32_t)(Com_Time_GetMs() - start_ms) >= ADC_CAL_TIMEOUT_MS)
        {
            ADC_Cmd(ADC1, DISABLE);
            return ERROR;
        }
    }

    return SUCCESS;
}
